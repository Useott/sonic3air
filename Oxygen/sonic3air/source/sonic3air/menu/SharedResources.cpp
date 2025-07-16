/*
*	Part of the Oxygen Engine / Sonic 3 A.I.R. software distribution.
*	Copyright (C) 2017-2025 by Eukaryot
*
*	Published under the GNU GPLv3 open source software license, see license.txt
*	or https://www.gnu.org/licenses/gpl-3.0.en.html
*/

#include "sonic3air/pch.h"
#include "sonic3air/menu/SharedResources.h"
#include "sonic3air/data/SharedDatabase.h"

#include "oxygen/application/EngineMain.h"
#include "oxygen/application/modding/ModManager.h"
#include "oxygen/helper/FileHelper.h"
#include "oxygen/helper/JsonHelper.h"
#include "oxygen/resources/FontCollection.h"


namespace global
{
	Font mSmallfont;
	Font mSmallfontSemiOutlined;
	Font mSmallfontRect;
	Font mOxyfontNarrowSimple;
	Font mOxyfontNarrow;
	Font mOxyfontTinySimple;
	Font mOxyfontTiny;
	Font mOxyfontTinyRect;
	Font mOxyfontSmallNoOutline;
	Font mOxyfontSmall;
	Font mMonofont;
	Font mOxyfontRegular;
	Font mSonicFontB;
	Font mSonicFontC;

	DrawerTexture mMainMenuBackgroundSeparator;
	DrawerTexture mDataSelectBackground;
	DrawerTexture mDataSelectAltBackground;
	DrawerTexture mLevelSelectBackground;
	DrawerTexture mOptionsTopBar;
	DrawerTexture mAchievementsFrame;
	DrawerTexture mTimeAttackResultsBG;

	std::map<uint32, DrawerTexture> mAchievementImage;
	std::map<uint32, DrawerTexture> mSecretImage;


	void loadSharedResources()
	{
		std::shared_ptr<ShadowFontProcessor> shadowFontProcessor  = std::make_shared<ShadowFontProcessor>(Vec2i(1, 1), 0.5f, 0.8f);
		std::shared_ptr<ShadowFontProcessor> shadowFontProcessor2 = std::make_shared<ShadowFontProcessor>(Vec2i(1, 1), 0.5f, 1.0f);
		std::shared_ptr<ShadowFontProcessor> shadowFontProcessor3 = std::make_shared<ShadowFontProcessor>(Vec2i(1, 1), 0.5f, 0.6f);
		std::shared_ptr<ShadowFontProcessor> shadowFontProcessor4 = std::make_shared<ShadowFontProcessor>(Vec2i(1, 1), 0.0f, 1.0f);

		std::shared_ptr<OutlineFontProcessor> outlineFontProcessor = std::make_shared<OutlineFontProcessor>();
		std::shared_ptr<OutlineFontProcessor> outlineFontProcessorTransparent = std::make_shared<OutlineFontProcessor>(Color(0.0f, 0.0f, 0.0f, 0.5f));
		std::shared_ptr<OutlineFontProcessor> outlineFontProcessorRect = std::make_shared<OutlineFontProcessor>(Color::BLACK, 1, true);

		std::shared_ptr<GradientFontProcessor> gradientFontProcessor = std::make_shared<GradientFontProcessor>();

		FontCollection& fontCollection = FontCollection::instance();

		fontCollection.registerManagedFont(mSmallfont, "smallfont");

		fontCollection.registerManagedFont(mSmallfontSemiOutlined, "smallfont");
		fontCollection.registerManagedFont(mSmallfontRect, "smallfont");
		fontCollection.registerManagedFont(mOxyfontNarrowSimple, "oxyfont_tiny_narrow");
		fontCollection.registerManagedFont(mOxyfontNarrow, "oxyfont_tiny_narrow");
		fontCollection.registerManagedFont(mOxyfontTinySimple, "oxyfont_tiny");
		fontCollection.registerManagedFont(mOxyfontTiny, "oxyfont_tiny");
		fontCollection.registerManagedFont(mOxyfontTinyRect, "oxyfont_tiny");
		fontCollection.registerManagedFont(mOxyfontSmallNoOutline, "oxyfont_small");
		fontCollection.registerManagedFont(mOxyfontSmall, "oxyfont_small");
		fontCollection.registerManagedFont(mOxyfontTiny, "oxyfont_tiny");
		fontCollection.registerManagedFont(mMonofont, "monofont");
		fontCollection.registerManagedFont(mOxyfontRegular, "oxyfont_regular");
		fontCollection.registerManagedFont(mSonicFontB, "sonic3_fontA");
		fontCollection.registerManagedFont(mSonicFontC, "sonic3_fontC");

		FileHelper::loadTexture(mDataSelectBackground, L"data/sprites/menu/main_menu/background.png");
		FileHelper::loadTexture(mDataSelectAltBackground, L"data/sprites/menu/main_menu/foreground.png");
		FileHelper::loadTexture(mOptionsTopBar, L"data/sprites/menu/main_menu/triangles.png");
		FileHelper::loadTexture(mTimeAttackResultsBG, L"data/images/menu/timeattack_results_screen.png");

		for (const SharedDatabase::Achievement& achievement : SharedDatabase::getAchievements())
		{
			const String filename(0, "data/images/achievements/%s.png", achievement.mImage.c_str());
			Bitmap bitmap;
			if (FileHelper::loadBitmap(bitmap, *filename.toWString()))
			{
				{
					DrawerTexture& texture = mAchievementImage[achievement.mType];
					if (!texture.isValid())
					{
						EngineMain::instance().getDrawer().createTexture(texture);
					}
					texture.accessBitmap() = bitmap;
					texture.bitmapUpdated();
				}

				// Convert to grayscale
				uint32* data = bitmap.getData();
				const int pixels = bitmap.getPixelCount();
				for (int i = 0; i < pixels; ++i)
				{
					data[i] = roundToInt(Color::fromABGR32(data[i]).getGray() * 255.0f) * 0x10101 + 0xff000000;
				}

				{
					DrawerTexture& texture = mAchievementImage[achievement.mType | 0x80000000];
					if (!texture.isValid())
					{
						EngineMain::instance().getDrawer().createTexture(texture);
					}
					texture.accessBitmap() = bitmap;
					texture.bitmapUpdated();
				}
			}
		}

		for (const SharedDatabase::Secret& secret : SharedDatabase::getSecrets())
		{
			if (!secret.mImage.empty())
			{
				const String filename(0, "data/images/secrets/%s.png", secret.mImage.c_str());
				FileHelper::loadTexture(mSecretImage[secret.mType], *filename.toWString());

				const String filename2(0, "data/images/secrets/%s_locked.png", secret.mImage.c_str());
				FileHelper::loadTexture(mSecretImage[secret.mType | 0x80000000], *filename2.toWString(), false);	// This is okay to fail for some secrets
			}
		}
	}
}
