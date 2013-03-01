/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "OpenGLRenderer"

#include <cutils/compiler.h>

#include <utils/JenkinsHash.h>

#include <SkGlyph.h>
#include <SkUtils.h>

#include "Debug.h"
#include "FontUtil.h"
#include "Font.h"
#include "FontRenderer.h"
#include "Properties.h"

namespace android {
namespace uirenderer {

///////////////////////////////////////////////////////////////////////////////
// Font
///////////////////////////////////////////////////////////////////////////////

Font::Font(FontRenderer* state, const Font::FontDescription& desc) :
        mState(state), mDescription(desc) {
}

Font::FontDescription::FontDescription(const SkPaint* paint, const mat4& matrix) {
    mFontId = SkTypeface::UniqueID(paint->getTypeface());
    mFontSize = paint->getTextSize();
    mFlags = 0;
    if (paint->isFakeBoldText()) {
        mFlags |= Font::kFakeBold;
    }
    mItalicStyle = paint->getTextSkewX();
    mScaleX = paint->getTextScaleX();
    mStyle = paint->getStyle();
    mStrokeWidth = paint->getStrokeWidth();
    mAntiAliasing = paint->isAntiAlias();
    mLookupTransform.reset();
    mLookupTransform[SkMatrix::kMScaleX] = matrix.data[mat4::kScaleX];
    mLookupTransform[SkMatrix::kMScaleY] = matrix.data[mat4::kScaleY];
    mLookupTransform[SkMatrix::kMSkewX] = matrix.data[mat4::kSkewX];
    mLookupTransform[SkMatrix::kMSkewY] = matrix.data[mat4::kSkewY];
}

Font::~Font() {
    mState->removeFont(this);

    for (uint32_t i = 0; i < mCachedGlyphs.size(); i++) {
        delete mCachedGlyphs.valueAt(i);
    }
}

hash_t Font::FontDescription::hash() const {
    uint32_t hash = JenkinsHashMix(0, mFontId);
    hash = JenkinsHashMix(hash, android::hash_type(mFontSize));
    hash = JenkinsHashMix(hash, android::hash_type(mFlags));
    hash = JenkinsHashMix(hash, android::hash_type(mItalicStyle));
    hash = JenkinsHashMix(hash, android::hash_type(mScaleX));
    hash = JenkinsHashMix(hash, android::hash_type(mStyle));
    hash = JenkinsHashMix(hash, android::hash_type(mStrokeWidth));
    hash = JenkinsHashMix(hash, int(mAntiAliasing));
    hash = JenkinsHashMix(hash, android::hash_type(mLookupTransform[SkMatrix::kMScaleX]));
    hash = JenkinsHashMix(hash, android::hash_type(mLookupTransform[SkMatrix::kMScaleY]));
    hash = JenkinsHashMix(hash, android::hash_type(mLookupTransform[SkMatrix::kMSkewX]));
    hash = JenkinsHashMix(hash, android::hash_type(mLookupTransform[SkMatrix::kMSkewY]));
    return JenkinsHashWhiten(hash);
}

int Font::FontDescription::compare(const Font::FontDescription& lhs,
        const Font::FontDescription& rhs) {
    int deltaInt = int(lhs.mFontId) - int(rhs.mFontId);
    if (deltaInt != 0) return deltaInt;

    if (lhs.mFontSize < rhs.mFontSize) return -1;
    if (lhs.mFontSize > rhs.mFontSize) return +1;

    if (lhs.mItalicStyle < rhs.mItalicStyle) return -1;
    if (lhs.mItalicStyle > rhs.mItalicStyle) return +1;

    deltaInt = int(lhs.mFlags) - int(rhs.mFlags);
    if (deltaInt != 0) return deltaInt;

    if (lhs.mScaleX < rhs.mScaleX) return -1;
    if (lhs.mScaleX > rhs.mScaleX) return +1;

    deltaInt = int(lhs.mStyle) - int(rhs.mStyle);
    if (deltaInt != 0) return deltaInt;

    if (lhs.mStrokeWidth < rhs.mStrokeWidth) return -1;
    if (lhs.mStrokeWidth > rhs.mStrokeWidth) return +1;

    deltaInt = int(lhs.mAntiAliasing) - int(rhs.mAntiAliasing);
    if (deltaInt != 0) return deltaInt;

    if (lhs.mLookupTransform[SkMatrix::kMScaleX] <
            rhs.mLookupTransform[SkMatrix::kMScaleX]) return -1;
    if (lhs.mLookupTransform[SkMatrix::kMScaleX] >
            rhs.mLookupTransform[SkMatrix::kMScaleX]) return +1;

    if (lhs.mLookupTransform[SkMatrix::kMScaleY] <
            rhs.mLookupTransform[SkMatrix::kMScaleY]) return -1;
    if (lhs.mLookupTransform[SkMatrix::kMScaleY] >
            rhs.mLookupTransform[SkMatrix::kMScaleY]) return +1;

    if (lhs.mLookupTransform[SkMatrix::kMSkewX] <
            rhs.mLookupTransform[SkMatrix::kMSkewX]) return -1;
    if (lhs.mLookupTransform[SkMatrix::kMSkewX] >
            rhs.mLookupTransform[SkMatrix::kMSkewX]) return +1;

    if (lhs.mLookupTransform[SkMatrix::kMSkewY] <
            rhs.mLookupTransform[SkMatrix::kMSkewY]) return -1;
    if (lhs.mLookupTransform[SkMatrix::kMSkewY] >
            rhs.mLookupTransform[SkMatrix::kMSkewY]) return +1;

    return 0;
}

void Font::invalidateTextureCache(CacheTexture* cacheTexture) {
    for (uint32_t i = 0; i < mCachedGlyphs.size(); i++) {
        CachedGlyphInfo* cachedGlyph = mCachedGlyphs.valueAt(i);
        if (!cacheTexture || cachedGlyph->mCacheTexture == cacheTexture) {
            cachedGlyph->mIsValid = false;
        }
    }
}

void Font::measureCachedGlyph(CachedGlyphInfo *glyph, int x, int y,
        uint8_t* bitmap, uint32_t bitmapW, uint32_t bitmapH, Rect* bounds, const float* pos) {
    int nPenX = x + glyph->mBitmapLeft;
    int nPenY = y + glyph->mBitmapTop;

    int width = (int) glyph->mBitmapWidth;
    int height = (int) glyph->mBitmapHeight;

    if (bounds->bottom > nPenY) {
        bounds->bottom = nPenY;
    }
    if (bounds->left > nPenX) {
        bounds->left = nPenX;
    }
    if (bounds->right < nPenX + width) {
        bounds->right = nPenX + width;
    }
    if (bounds->top < nPenY + height) {
        bounds->top = nPenY + height;
    }
}

void Font::drawCachedGlyph(CachedGlyphInfo* glyph, int x, int y,
        uint8_t* bitmap, uint32_t bitmapW, uint32_t bitmapH, Rect* bounds, const float* pos) {
    float nPenX = x + glyph->mBitmapLeft;
    float nPenY = y + glyph->mBitmapTop + glyph->mBitmapHeight;

    float width = (float) glyph->mBitmapWidth;
    float height = (float) glyph->mBitmapHeight;

    float u1 = glyph->mBitmapMinU;
    float u2 = glyph->mBitmapMaxU;
    float v1 = glyph->mBitmapMinV;
    float v2 = glyph->mBitmapMaxV;

    mState->appendMeshQuad(nPenX, nPenY, u1, v2,
            nPenX + width, nPenY, u2, v2,
            nPenX + width, nPenY - height, u2, v1,
            nPenX, nPenY - height, u1, v1, glyph->mCacheTexture);
}

void Font::drawCachedGlyphPerspective(CachedGlyphInfo* glyph, int x, int y,
        uint8_t* bitmap, uint32_t bitmapW, uint32_t bitmapH, Rect* bounds, const float* pos) {
    SkMatrix i;
    if (!mDescription.mLookupTransform.invert(&i)) {
        return;
    }

    SkPoint p[4];
    p[0].set(glyph->mBitmapLeft, glyph->mBitmapTop + glyph->mBitmapHeight);
    p[1].set(glyph->mBitmapLeft + glyph->mBitmapWidth, glyph->mBitmapTop + glyph->mBitmapHeight);
    p[2].set(glyph->mBitmapLeft + glyph->mBitmapWidth, glyph->mBitmapTop);
    p[3].set(glyph->mBitmapLeft, glyph->mBitmapTop);

    i.mapPoints(p, 4);

    p[0].offset(x, y);
    p[1].offset(x, y);
    p[2].offset(x, y);
    p[3].offset(x, y);

    float u1 = glyph->mBitmapMinU;
    float u2 = glyph->mBitmapMaxU;
    float v1 = glyph->mBitmapMinV;
    float v2 = glyph->mBitmapMaxV;

    mState->appendRotatedMeshQuad(
            p[0].fX, p[0].fY, u1, v2,
            p[1].fX, p[1].fY, u2, v2,
            p[2].fX, p[2].fY, u2, v1,
            p[3].fX, p[3].fY, u1, v1, glyph->mCacheTexture);
}

void Font::drawCachedGlyphBitmap(CachedGlyphInfo* glyph, int x, int y,
        uint8_t* bitmap, uint32_t bitmapW, uint32_t bitmapH, Rect* bounds, const float* pos) {
    int nPenX = x + glyph->mBitmapLeft;
    int nPenY = y + glyph->mBitmapTop;

    uint32_t endX = glyph->mStartX + glyph->mBitmapWidth;
    uint32_t endY = glyph->mStartY + glyph->mBitmapHeight;

    CacheTexture* cacheTexture = glyph->mCacheTexture;
    uint32_t cacheWidth = cacheTexture->getWidth();
    const uint8_t* cacheBuffer = cacheTexture->getTexture();

    uint32_t cacheX = 0, cacheY = 0;
    int32_t bX = 0, bY = 0;
    for (cacheX = glyph->mStartX, bX = nPenX; cacheX < endX; cacheX++, bX++) {
        for (cacheY = glyph->mStartY, bY = nPenY; cacheY < endY; cacheY++, bY++) {
            uint8_t tempCol = cacheBuffer[cacheY * cacheWidth + cacheX];
            bitmap[bY * bitmapW + bX] = tempCol;
        }
    }
}

void Font::drawCachedGlyph(CachedGlyphInfo* glyph, float x, float hOffset, float vOffset,
        SkPathMeasure& measure, SkPoint* position, SkVector* tangent) {
    const float halfWidth = glyph->mBitmapWidth * 0.5f;
    const float height = glyph->mBitmapHeight;

    vOffset += glyph->mBitmapTop + height;

    SkPoint destination[4];
    bool ok = measure.getPosTan(x + hOffset + glyph->mBitmapLeft + halfWidth, position, tangent);
    if (!ok) {
        ALOGW("The path for drawTextOnPath is empty or null");
    }

    // Move along the tangent and offset by the normal
    destination[0].set(-tangent->fX * halfWidth - tangent->fY * vOffset,
            -tangent->fY * halfWidth + tangent->fX * vOffset);
    destination[1].set(tangent->fX * halfWidth - tangent->fY * vOffset,
            tangent->fY * halfWidth + tangent->fX * vOffset);
    destination[2].set(destination[1].fX + tangent->fY * height,
            destination[1].fY - tangent->fX * height);
    destination[3].set(destination[0].fX + tangent->fY * height,
            destination[0].fY - tangent->fX * height);

    const float u1 = glyph->mBitmapMinU;
    const float u2 = glyph->mBitmapMaxU;
    const float v1 = glyph->mBitmapMinV;
    const float v2 = glyph->mBitmapMaxV;

    mState->appendRotatedMeshQuad(
            position->fX + destination[0].fX,
            position->fY + destination[0].fY, u1, v2,
            position->fX + destination[1].fX,
            position->fY + destination[1].fY, u2, v2,
            position->fX + destination[2].fX,
            position->fY + destination[2].fY, u2, v1,
            position->fX + destination[3].fX,
            position->fY + destination[3].fY, u1, v1,
            glyph->mCacheTexture);
}

CachedGlyphInfo* Font::getCachedGlyph(SkPaint* paint, glyph_t textUnit, bool precaching) {
    CachedGlyphInfo* cachedGlyph = NULL;
    ssize_t index = mCachedGlyphs.indexOfKey(textUnit);
    if (index >= 0) {
        cachedGlyph = mCachedGlyphs.valueAt(index);

        // Is the glyph still in texture cache?
        if (!cachedGlyph->mIsValid) {
            const SkGlyph& skiaGlyph = GET_METRICS(paint, textUnit,
                    &mDescription.mLookupTransform);
            updateGlyphCache(paint, skiaGlyph, cachedGlyph, precaching);
        }
    } else {
        cachedGlyph = cacheGlyph(paint, textUnit, precaching);
    }

    return cachedGlyph;
}

void Font::render(SkPaint* paint, const char *text, uint32_t start, uint32_t len,
            int numGlyphs, int x, int y, const float* positions) {
    render(paint, text, start, len, numGlyphs, x, y, FRAMEBUFFER, NULL,
            0, 0, NULL, positions);
}

void Font::render(SkPaint* paint, const char *text, uint32_t start, uint32_t len,
        int numGlyphs, SkPath* path, float hOffset, float vOffset) {
    if (numGlyphs == 0 || text == NULL || len == 0) {
        return;
    }

    text += start;

    int glyphsCount = 0;
    SkFixed prevRsbDelta = 0;

    float penX = 0.0f;

    SkPoint position;
    SkVector tangent;

    SkPathMeasure measure(*path, false);
    float pathLength = SkScalarToFloat(measure.getLength());

    if (paint->getTextAlign() != SkPaint::kLeft_Align) {
        float textWidth = SkScalarToFloat(paint->measureText(text, len));
        float pathOffset = pathLength;
        if (paint->getTextAlign() == SkPaint::kCenter_Align) {
            textWidth *= 0.5f;
            pathOffset *= 0.5f;
        }
        penX += pathOffset - textWidth;
    }

    while (glyphsCount < numGlyphs && penX < pathLength) {
        glyph_t glyph = GET_GLYPH(text);

        if (IS_END_OF_STRING(glyph)) {
            break;
        }

        CachedGlyphInfo* cachedGlyph = getCachedGlyph(paint, glyph);
        penX += SkFixedToFloat(AUTO_KERN(prevRsbDelta, cachedGlyph->mLsbDelta));
        prevRsbDelta = cachedGlyph->mRsbDelta;

        if (cachedGlyph->mIsValid && cachedGlyph->mCacheTexture) {
            drawCachedGlyph(cachedGlyph, penX, hOffset, vOffset, measure, &position, &tangent);
        }

        penX += SkFixedToFloat(cachedGlyph->mAdvanceX);

        glyphsCount++;
    }
}

void Font::measure(SkPaint* paint, const char* text, uint32_t start, uint32_t len,
        int numGlyphs, Rect *bounds, const float* positions) {
    if (bounds == NULL) {
        ALOGE("No return rectangle provided to measure text");
        return;
    }
    bounds->set(1e6, -1e6, -1e6, 1e6);
    render(paint, text, start, len, numGlyphs, 0, 0, MEASURE, NULL, 0, 0, bounds, positions);
}

void Font::precache(SkPaint* paint, const char* text, int numGlyphs) {
    if (numGlyphs == 0 || text == NULL) {
        return;
    }
    int glyphsCount = 0;

    while (glyphsCount < numGlyphs) {
        glyph_t glyph = GET_GLYPH(text);

        // Reached the end of the string
        if (IS_END_OF_STRING(glyph)) {
            break;
        }

        CachedGlyphInfo* cachedGlyph = getCachedGlyph(paint, glyph, true);

        glyphsCount++;
    }
}

void Font::render(SkPaint* paint, const char* text, uint32_t start, uint32_t len,
        int numGlyphs, int x, int y, RenderMode mode, uint8_t *bitmap,
        uint32_t bitmapW, uint32_t bitmapH, Rect* bounds, const float* positions) {
    if (numGlyphs == 0 || text == NULL || len == 0) {
        return;
    }

    static RenderGlyph gRenderGlyph[] = {
            &android::uirenderer::Font::drawCachedGlyph,
            &android::uirenderer::Font::drawCachedGlyphPerspective,
            &android::uirenderer::Font::drawCachedGlyphBitmap,
            &android::uirenderer::Font::drawCachedGlyphBitmap,
            &android::uirenderer::Font::measureCachedGlyph,
            &android::uirenderer::Font::measureCachedGlyph
    };
    RenderGlyph render = gRenderGlyph[(mode << 1) + mTransform.isPerspective()];

    text += start;
    int glyphsCount = 0;

    const bool applyTransform = !mTransform.isIdentity() && !mTransform.isPerspective();
    const SkPaint::Align align = paint->getTextAlign();

    while (glyphsCount < numGlyphs) {
        glyph_t glyph = GET_GLYPH(text);

        // Reached the end of the string
        if (IS_END_OF_STRING(glyph)) {
            break;
        }

        CachedGlyphInfo* cachedGlyph = getCachedGlyph(paint, glyph);

        // If it's still not valid, we couldn't cache it, so we shouldn't
        // draw garbage; also skip empty glyphs (spaces)
        if (cachedGlyph->mIsValid && cachedGlyph->mCacheTexture) {
            float penX = x + positions[(glyphsCount << 1)];
            float penY = y + positions[(glyphsCount << 1) + 1];

            if (applyTransform) {
                mTransform.mapPoint(penX, penY);
            }

            (*this.*render)(cachedGlyph, roundf(penX), roundf(penY),
                    bitmap, bitmapW, bitmapH, bounds, positions);
        }

        glyphsCount++;
    }
}

void Font::updateGlyphCache(SkPaint* paint, const SkGlyph& skiaGlyph, CachedGlyphInfo* glyph,
        bool precaching) {
    glyph->mAdvanceX = skiaGlyph.fAdvanceX;
    glyph->mAdvanceY = skiaGlyph.fAdvanceY;
    glyph->mBitmapLeft = skiaGlyph.fLeft;
    glyph->mBitmapTop = skiaGlyph.fTop;
    glyph->mLsbDelta = skiaGlyph.fLsbDelta;
    glyph->mRsbDelta = skiaGlyph.fRsbDelta;

    uint32_t startX = 0;
    uint32_t startY = 0;

    // Get the bitmap for the glyph
    if (!skiaGlyph.fImage) {
        paint->findImage(skiaGlyph, &mDescription.mLookupTransform);
    }
    mState->cacheBitmap(skiaGlyph, glyph, &startX, &startY, precaching);

    if (!glyph->mIsValid) {
        return;
    }

    uint32_t endX = startX + skiaGlyph.fWidth;
    uint32_t endY = startY + skiaGlyph.fHeight;

    glyph->mStartX = startX;
    glyph->mStartY = startY;
    glyph->mBitmapWidth = skiaGlyph.fWidth;
    glyph->mBitmapHeight = skiaGlyph.fHeight;

    bool empty = skiaGlyph.fWidth == 0 || skiaGlyph.fHeight == 0;
    if (!empty) {
        uint32_t cacheWidth = glyph->mCacheTexture->getWidth();
        uint32_t cacheHeight = glyph->mCacheTexture->getHeight();

        glyph->mBitmapMinU = startX / (float) cacheWidth;
        glyph->mBitmapMinV = startY / (float) cacheHeight;
        glyph->mBitmapMaxU = endX / (float) cacheWidth;
        glyph->mBitmapMaxV = endY / (float) cacheHeight;

        mState->setTextureDirty();
    }
}

CachedGlyphInfo* Font::cacheGlyph(SkPaint* paint, glyph_t glyph, bool precaching) {
    CachedGlyphInfo* newGlyph = new CachedGlyphInfo();
    mCachedGlyphs.add(glyph, newGlyph);

    const SkGlyph& skiaGlyph = GET_METRICS(paint, glyph, &mDescription.mLookupTransform);
    newGlyph->mIsValid = false;
    newGlyph->mGlyphIndex = skiaGlyph.fID;

    updateGlyphCache(paint, skiaGlyph, newGlyph, precaching);

    return newGlyph;
}

Font* Font::create(FontRenderer* state, const SkPaint* paint, const mat4& matrix) {
    FontDescription description(paint, matrix);
    Font* font = state->mActiveFonts.get(description);

    if (!font) {
        font = new Font(state, description);
        state->mActiveFonts.put(description, font);
    }
    font->mTransform.load(matrix);

    return font;
}

}; // namespace uirenderer
}; // namespace android
