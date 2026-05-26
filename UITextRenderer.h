#pragma once

#include "main.h"
#include <d2d1_1.h>
#include <dwrite.h>
#include <string>
#include <wrl/client.h>

class UITextRenderer
{
public:
	static bool Init();
	static void Uninit();

	static void Begin();
	static void End();

	static void DrawTextUtf8(
		const std::string& text,
		float x,
		float y,
		float width,
		float height,
		float fontSize,
		const D2D1_COLOR_F& color);

	static void DrawOutlinedTextUtf8(
		const std::string& text,
		float x,
		float y,
		float width,
		float height,
		float fontSize,
		const D2D1_COLOR_F& color);

	static void DrawTextUtf8Aligned(
		const std::string& text,
		float x,
		float y,
		float width,
		float height,
		float fontSize,
		const D2D1_COLOR_F& color,
		DWRITE_TEXT_ALIGNMENT textAlignment,
		DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

	static void DrawOutlinedTextUtf8Aligned(
		const std::string& text,
		float x,
		float y,
		float width,
		float height,
		float fontSize,
		const D2D1_COLOR_F& color,
		DWRITE_TEXT_ALIGNMENT textAlignment,
		DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment = DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

private:
	static std::wstring ToWide(const std::string& text);

	static Microsoft::WRL::ComPtr<ID2D1Factory1> m_D2DFactory;
	static Microsoft::WRL::ComPtr<ID2D1Device> m_D2DDevice;
	static Microsoft::WRL::ComPtr<ID2D1DeviceContext> m_D2DContext;
	static Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_TargetBitmap;
	static Microsoft::WRL::ComPtr<IDWriteFactory> m_DWriteFactory;
	static bool m_IsDrawing;
};
