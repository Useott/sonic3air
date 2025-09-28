/*
*	Part of the Oxygen Engine / Sonic 3 A.I.R. software distribution.
*	Copyright (C) 2017-2025 by Eukaryot
*
*	Published under the GNU GPLv3 open source software license, see license.txt
*	or https://www.gnu.org/licenses/gpl-3.0.en.html
*/

#include "sonic3air/pch.h"
#include "sonic3air/menu/MenuBackground.h"
#include "sonic3air/menu/ExtrasMenu.h"
#include "sonic3air/menu/GameApp.h"
#include "sonic3air/menu/GameMenuManager.h"
#include "sonic3air/menu/MainMenu.h"
#include "sonic3air/menu/SharedResources.h"
#include "sonic3air/menu/TimeAttackMenu.h"
#include "sonic3air/menu/mods/ModsMenu.h"
#include "sonic3air/menu/options/OptionsMenu.h"
#include "sonic3air/Game.h"

#include "oxygen/application/Application.h"
#include "oxygen/application/EngineMain.h"
#include "oxygen/application/gameview/GameView.h"
#include "oxygen/application/video/VideoOut.h"
#include "oxygen/simulation/CodeExec.h"
#include "oxygen/simulation/EmulatorInterface.h"
#include "oxygen/simulation/Simulation.h"


namespace detail
{
	template<typename T>
	void createGameMenuInstance(T*& gameMenu, std::vector<GameMenuBase*>& allGameMenus, MenuBackground& menuBackground)
	{
		gameMenu = new T(menuBackground);
		allGameMenus.push_back(gameMenu);
	}

	void drawQuad(Drawer& drawer, float splitX1, float splitX2, DrawerTexture& texture)
	{
		DrawerMeshVertex quad[4];
		quad[0].mPosition.set(splitX1 + 15.0f, 0.0f);
		quad[1].mPosition.set(splitX2 + 15.0f, 0.0f);
		quad[2].mPosition.set(splitX1 - 15.0f, 224.0f);
		quad[3].mPosition.set(splitX2 - 15.0f, 224.0f);

		for (int i = 0; i < 4; ++i)
		{
			quad[i].mTexcoords.x = (quad[i].mPosition.x + 8.0f) / 416.0f;
			quad[i].mTexcoords.y = (quad[i].mPosition.y) / 224.0f;
		}
		drawer.drawQuad(quad, texture);
	}

	void drawSeparator(Drawer& drawer, float splitX, float animationOffset, bool mirrored)
	{
		DrawerMeshVertex quad[4];
		if (mirrored)
		{
			quad[0].mPosition.set(splitX + 10.0f, -3.0f);
			quad[1].mPosition.set(splitX + 25.0f,  0.0f);
			quad[2].mPosition.set(splitX - 20.0f, 224.0f);
			quad[3].mPosition.set(splitX -  5.0f, 227.0f);

			quad[0].mTexcoords.set(1.0f, animationOffset);
			quad[1].mTexcoords.set(0.0f, animationOffset);
			quad[2].mTexcoords.set(1.0f, animationOffset + 16.0f);
			quad[3].mTexcoords.set(0.0f, animationOffset + 16.0f);
		}
		else
		{
			quad[0].mPosition.set(splitX +  5.0f, -3.0f);
			quad[1].mPosition.set(splitX + 20.0f,  0.0f);
			quad[2].mPosition.set(splitX - 25.0f, 224.0f);
			quad[3].mPosition.set(splitX - 10.0f, 227.0f);

			quad[0].mTexcoords.set(0.0f, animationOffset);
			quad[1].mTexcoords.set(1.0f, animationOffset);
			quad[2].mTexcoords.set(0.0f, animationOffset + 16.0f);
			quad[3].mTexcoords.set(1.0f, animationOffset + 16.0f);
		}
		drawer.drawQuad(quad, global::mMainMenuBackgroundSeparator);
	}
}


MenuBackground::MenuBackground()
{
	detail::createGameMenuInstance(mMainMenu,		mAllChildren, *this);
	detail::createGameMenuInstance(mTimeAttackMenu,	mAllChildren, *this);
	detail::createGameMenuInstance(mOptionsMenu,	mAllChildren, *this);
	detail::createGameMenuInstance(mExtrasMenu,		mAllChildren, *this);
	detail::createGameMenuInstance(mModsMenu,		mAllChildren, *this);
}

MenuBackground::~MenuBackground()
{
	for (GameMenuBase* child : mAllChildren)
	{
		delete child;
	}
}

void MenuBackground::initialize()
{
	// On first initialize, build the preview sprite keys
	if (mPreviewSprites.empty())
	{
		const std::vector<SharedDatabase::Zone>& zones = SharedDatabase::getAllZones();
		for (const SharedDatabase::Zone& zone : zones)
		{
			const uint8 acts = std::max(zone.mActsNormal, zone.mActsTimeAttack);
			if (acts == 0)
				continue;

			PreviewKey key;
			key.mZone = zone.mInternalIndex;
			for (uint8 act = 0; act < acts; ++act)
			{
				key.mAct = act;
				for (uint8 image = 0; image < 2; ++image)
				{
					key.mImage = image;
					const String spriteName(0, "%s_act%d%c", zone.mShortName.substr(0, 6).c_str(), act + 1, 'a' + image);

					PreviewSprite& previewSprite = mPreviewSprites[key];
					previewSprite.mSpriteKey = rmx::getMurmur2_64(spriteName);
					previewSprite.mPaletteKey = previewSprite.mSpriteKey;
				}
			}
		}
	}

	mLightLayer.setPosition(1.0f);
	mBlueLayer.setPosition(1.0f);
	mAlterLayer.setPosition(0.0f);
	mBackgroundLayer.setPosition(0.0f);

	mTarget = Target::TITLE;
	startTransition(Target::SPLIT);

	mAnimationTimer = 0.0f;
	setPreviewZoneAndAct(0, 0, true);

	// Do not automatically open a menu here, that always needs to be done separately via a call like "openMainMenu"
}

void MenuBackground::deinitialize()
{
	for (GameMenuBase* child : mAllChildren)
	{
		removeChild(*child);
	}
}

void MenuBackground::update(float timeElapsed)
{
	GuiBase::update(timeElapsed);

	mAnimationTimer += timeElapsed;
	if (mAnimationTimer >= 60.0f)
		mAnimationTimer -= 60.0f;

	updatePreview(timeElapsed);
	updateTransition(timeElapsed);
}

void MenuBackground::render()
{
	Drawer& drawer = EngineMain::instance().getDrawer();

	for (GameMenuBase* child : mAllChildren)
	{
		child->setRect(mRect);
	}

	mAnimationTimerAIT += 1;

	// Layers
	{
		int px = -(mAnimationTimerAIT / 8) % 512;
		int py = int(3.0f * sin(2 * 3.1415 * 128 * float(uint8(mAnimationTimerAIT)))) - 8;

		const std::time_t current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		char time_text[64];
		strftime(time_text, sizeof time_text, std::string("%m").c_str(), std::localtime(&current_time));
		int month = rmx::parseInteger(time_text);

		if (month == 12)
		{
			int px2 = -(mAnimationTimerAIT / 4) % 512;
			drawer.drawSprite(Vec2i(0, 0), rmx::getMurmur2_64("main_menu.background_december"), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));

			drawer.pushScissor(Recti(0, 180, VideoOut::instance().getScreenWidth(), 16));

			drawer.drawSprite(Vec2i(px, 0), rmx::getMurmur2_64("main_menu.background_december"), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));
			drawer.drawSprite(Vec2i(px + 512, 0), rmx::getMurmur2_64("main_menu.background_december"), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));

			drawer.popScissor();
			drawer.pushScissor(Recti(0, 196, VideoOut::instance().getScreenWidth(), 60));

			drawer.drawSprite(Vec2i(px2, 0), rmx::getMurmur2_64("main_menu.background_december"), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));
			drawer.drawSprite(Vec2i(px2 + 512, 0), rmx::getMurmur2_64("main_menu.background_december"), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));

			drawer.popScissor();

			drawer.drawSprite(Vec2i(0, 0), rmx::getMurmur2_64("main_menu.foreground_december"), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));
		}
		else if (month == 10)
		{
			drawer.drawSprite(Vec2i(px, 0), rmx::getMurmur2_64("main_menu.background_october"), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));
			drawer.drawSprite(Vec2i(px + 512, 0), rmx::getMurmur2_64("main_menu.background_october"), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));

			for (int i = 0; i < 64; ++i)
			{
				float waveMul = 0.125f + (1.0f - 0.125f) * (float(i) / 64.0f);
				px = -(mAnimationTimerAIT / 8) % 512 + std::round((10.0f * waveMul) * std::sin(2.0f * 3.1415f * 384 * float(i * 4) + float(mAnimationTimerAIT) / 41.0f));
				drawer.pushScissor(Recti(0, 160 + i, VideoOut::instance().getScreenWidth(), i));
				drawer.drawSprite(Vec2i(px % 512, 0), rmx::getMurmur2_64("main_menu.background_october"), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));
				drawer.drawSprite(Vec2i(px % 512 + 512, 0), rmx::getMurmur2_64("main_menu.background_october"), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));
				drawer.popScissor();
			}
		}
		else
		{
			drawer.drawSprite(Vec2i(px, py), rmx::getMurmur2_64("main_menu.background"), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));
			drawer.drawSprite(Vec2i(px + 512, py), rmx::getMurmur2_64("main_menu.background"), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));
			drawer.drawSprite(Vec2i(0, 0), rmx::getMurmur2_64("main_menu.foreground"), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));
		}

		int triangleScroll = mAnimationTimerAIT;
		const int scrHeight = VideoOut::instance().getScreenHeight();
		std::string triangle_key = "main_menu.triangles";
		if (month == 10)
			triangle_key = "main_menu.triangles_october";
		drawer.drawSprite(Vec2i(triangleScroll % 256, 0), rmx::getMurmur2_64(triangle_key), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));
		drawer.drawSprite(Vec2i((triangleScroll % 256) + 512, 0), rmx::getMurmur2_64(triangle_key), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));
		drawer.drawSprite(Vec2i(-(triangleScroll % 256), scrHeight), rmx::getMurmur2_64(triangle_key), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));
		drawer.drawSprite(Vec2i(-(triangleScroll % 256) + 512, scrHeight), rmx::getMurmur2_64(triangle_key), Color(1.0f, 1.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f));
	}

	if (mPreviewVisibility > 0.0f)
	{
		for (int i = 0; i < 2; ++i)
		{
			const PreviewImage& img = mPreviewImage[i];
			if (nullptr == img.mPreviewSprite)
				continue;

			const int maxOffset = std::min(480 - (int)mRect.width, 80);
			const int visibleWidth = roundToInt(mRect.width * img.mVisibility);

			drawer.pushScissor(Recti((int)mRect.width - visibleWidth, 0, visibleWidth, (int)mRect.height));
			drawer.drawSprite(Vec2i(int16(VideoOut::instance().getScreenWidth() - 480) / 2, 26), img.mPreviewSprite->mSpriteKey, img.mPreviewSprite->mPaletteKey, Color(1.0f, 1.0f, 1.0f, mPreviewVisibility));
			drawer.popScissor();
		}

		drawer.drawSprite(Vec2i(0, 16), rmx::constMurmur2_64("level_preview_border_left"), Color(1.0f, 1.0f, 1.0f, mPreviewVisibility));
		drawer.drawSpriteRect(Recti(10, 16, (int)mRect.width - 20, 100), rmx::constMurmur2_64("level_preview_border_center"), Color(1.0f, 1.0f, 1.0f, mPreviewVisibility));
		drawer.drawSprite(Vec2i((int)mRect.width, 16), rmx::constMurmur2_64("level_preview_border_right"), Color(1.0f, 1.0f, 1.0f, mPreviewVisibility));
	}

	GuiBase::render();

	drawer.performRendering();
}

void MenuBackground::startTransition(Target target)
{
	if (target == mTarget)
		return;

	// Set defaults, to be overwritten below
	mLightLayer.mTargetPosition = 1.0f;
	mBlueLayer.mTargetPosition = 1.5f;		// Far to the right
	mAlterLayer.mTargetPosition = -1.0f;	// Far to the left
	mBackgroundLayer.mTargetPosition = 0.0f;
	mLightLayer.mDelay = 0.0f;
	mBlueLayer.mDelay = 0.0f;
	mAlterLayer.mDelay = 0.0f;
	mBackgroundLayer.mDelay = 0.0f;

	switch (target)
	{
		case Target::TITLE:
		{
			// Only used when exiting the application
			mLightLayer.mDelay = 0.1f;
			break;
		}

		case Target::SPLIT:
		{
			mLightLayer.mTargetPosition = 0.465f;
			break;
		}

		case Target::LIGHT:
		{
			mLightLayer.mTargetPosition = 0.0f;
			mBackgroundLayer.mTargetPosition = -0.5f;
			break;
		}

		case Target::BLUE:
		{
			mBlueLayer.mTargetPosition = 0.0f;
			mLightLayer.mTargetPosition = -0.5f;	// Far to the left
			mBackgroundLayer.mTargetPosition = -0.5f;
			break;
		}

		case Target::ALTER:
		{
			mLightLayer.mTargetPosition = 1.5f;
			mLightLayer.mDelay = 0.1f;
			mAlterLayer.mTargetPosition = 1.0f;
			mBackgroundLayer.mTargetPosition = 1.5f;
			mBackgroundLayer.mDelay = 0.1f;
			break;
		}

		default:
			break;
	}

	mTarget = target;
	mInTransition = true;
}

void MenuBackground::setPreviewZoneAndAct(uint8 zone, uint8 act, bool forceReset)
{
	if ((mPreviewKey.mZone == zone && mPreviewKey.mAct == act) && !forceReset)
		return;

	mPreviewKey.mZone = zone;
	mPreviewKey.mAct = act;
	mPreviewKey.mImage = 0;

	mPreviewImage[0].mPreviewSprite = &mPreviewSprites[mPreviewKey];
	mPreviewImage[0].mSubIndex = 0;
	mPreviewImage[0].mOffset = 0.5f;
	mPreviewImage[0].mVisibility = 1.0f;
	mPreviewImage[1].mPreviewSprite = nullptr;

	mCurrentTime = 0.0f;
	updatePreview(0.0f);
}

void MenuBackground::showPreview(bool show, bool useTransition)
{
	if (useTransition)
	{
		mPreviewVisibilityChange = show ? 10.0f : -10.0f;
	}
	else
	{
		mPreviewVisibility = show ? 1.0f : 0.0f;
		mPreviewVisibilityChange = 0.0f;
	}
}

void MenuBackground::openMainMenu()
{
	openMenu(*mMainMenu);
}

void MenuBackground::openTimeAttackMenu()
{
	openMenu(*mTimeAttackMenu);
	skipTransition();
	mGameStartedMenu = mMainMenu;
	mGameStartedMenu->setBaseState(GameMenuBase::BaseState::INACTIVE);
}

void MenuBackground::openOptions(uint8 enteredInGame)
{
	openMenu(*mOptionsMenu);

	if (enteredInGame)
	{
		skipTransition();
		mGameStartedMenu = mMainMenu;
		mGameStartedMenu->setBaseState(GameMenuBase::BaseState::INACTIVE);
	}

	mOptionsMenu->setupOptionsMenu(enteredInGame);
	showPreview(false, !enteredInGame);
}

void MenuBackground::openExtras()
{
	openMenu(*mExtrasMenu);
}

void MenuBackground::openMods()
{
	openMenu(*mModsMenu);
	skipTransition();
	mGameStartedMenu = mMainMenu;
	mGameStartedMenu->setBaseState(GameMenuBase::BaseState::INACTIVE);
}

void MenuBackground::openGameStartedMenu()
{
	// Open either the menu that started the last in-game session (should be either the Main Menu, Act Select, Time Attack, or Extras)
	if (nullptr != mGameStartedMenu && mGameStartedMenu != mMainMenu)
	{
		openMenu(*mGameStartedMenu);
		skipTransition();
	}
	else
	{
		openMenu(*mMainMenu);
	}
}

void MenuBackground::setGameStartedMenu()
{
	mGameStartedMenu = mLastOpenedMenu;
}

void MenuBackground::openMenu(GameMenuBase& menu)
{
	// The menus only really work in a fixed resolution, so make sure that one is set
	//VideoOut::instance().setScreenSize(400, 224);

	GameMenuManager::instance().addMenu(menu);

	mLastOpenedMenu = &menu;
}

void MenuBackground::skipTransition()
{
	// Skip the transition entirely
	if (mInTransition)
	{
		Layer* layers[4] = { &mLightLayer, &mBlueLayer, &mAlterLayer, &mBackgroundLayer };
		for (Layer* layer : layers)
		{
			layer->mDelay = 0.0f;
			layer->mCurrentPosition = layer->mTargetPosition;
		}
		mInTransition = false;
	}
}

void MenuBackground::updateTransition(float timeElapsed)
{
	if (mInTransition)
	{
		mInTransition = false;

		// Update layer movement
		Layer* layers[4] = { &mLightLayer, &mBlueLayer, &mAlterLayer, &mBackgroundLayer };
		for (Layer* layer : layers)
		{
			if (layer->mDelay > 0.0f)
			{
				layer->mDelay = std::max(layer->mDelay - timeElapsed, 0.0f);
				mInTransition = true;
			}
			else if (layer->mCurrentPosition != layer->mTargetPosition)
			{
				if (layer->mCurrentPosition < layer->mTargetPosition)
				{
					layer->mCurrentPosition = std::min(layer->mCurrentPosition + timeElapsed * layer->mMoveSpeed, layer->mTargetPosition);
				}
				else
				{
					layer->mCurrentPosition = std::max(layer->mCurrentPosition - timeElapsed * layer->mMoveSpeed, layer->mTargetPosition);
				}
				mInTransition = true;
			}
		}
	}
}

void MenuBackground::updatePreview(float timeElapsed)
{
	if (timeElapsed > 0.0f && mPreviewVisibilityChange != 0.0f)
	{
		mPreviewVisibility += mPreviewVisibilityChange * timeElapsed;
		if (mPreviewVisibility <= 0.0f)
		{
			mPreviewVisibility = 0.0f;
			mPreviewVisibilityChange = 0.0f;
			setPreviewZoneAndAct(mPreviewKey.mZone, mPreviewKey.mAct, true);
		}
		else if (mPreviewVisibility >= 1.0f)
		{
			mPreviewVisibility = 1.0f;
			mPreviewVisibilityChange = 0.0f;
		}
	}

	if (mPreviewVisibility > 0.0f)
	{
		constexpr float MOVE_TIME = 2.5f;
		constexpr float CHANGE_TIME = 0.6f;
		constexpr float TOTAL_TIME = MOVE_TIME + CHANGE_TIME;

		mCurrentTime += timeElapsed;
		if (mCurrentTime >= TOTAL_TIME)
		{
			// Time to restart
			mPreviewImage[0] = mPreviewImage[1];
			mPreviewImage[0].mOffset = 0.5f;
			mPreviewImage[0].mVisibility = 1.0f;
			mPreviewImage[1].mPreviewSprite = nullptr;
			mCurrentTime -= TOTAL_TIME;
		}

		if (mCurrentTime >= MOVE_TIME)
		{
			// Transition animation
			if (nullptr == mPreviewImage[1].mPreviewSprite)
			{
				mPreviewKey.mImage = (mPreviewImage[0].mSubIndex + 1) % 2;
				mPreviewImage[1].mPreviewSprite = &mPreviewSprites[mPreviewKey];
				mPreviewImage[1].mSubIndex = mPreviewKey.mImage;
			}

			const float animtime = (mCurrentTime - MOVE_TIME) / CHANGE_TIME;
			const float offset = (1.0f - std::cos(animtime * PI_FLOAT)) * 0.25f;
			mPreviewImage[0].mOffset = 0.5f + offset;

			mPreviewImage[1].mOffset = offset;
			mPreviewImage[1].mVisibility = animtime;
		}
	}
}

std::string MenuBackground::askLemonScriptNicelyForATranslatedString(std::string key, uint8 id)
{
	CodeExec::FunctionExecData execData;
	execData.addParam(lemon::PredefinedDataTypes::UINT_64, rmx::getMurmur2_64(key));
	execData.addParam(lemon::PredefinedDataTypes::UINT_8, id + 1);
	Application::instance().getSimulation().getCodeExec().executeScriptFunction("Standalone.getTranslatedStringForCXX", false, &execData);

	const lemon::AnyBaseValue value = Application::instance().getSimulation().getCodeExec().getLemonScriptRuntime().getGlobalVariableValue("lastTranslatedString", &lemon::PredefinedDataTypes::STRING);
	lemon::StringRef text = lemon::StringRef(value.get<uint64>());
	return std::string{text.getString()};
}
