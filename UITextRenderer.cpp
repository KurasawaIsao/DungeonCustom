#include "UITextRenderer.h"
#include "Renderer.h"

#include <dxgi.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

using Microsoft::WRL::ComPtr;

ComPtr<ID2D1Factory1> UITextRenderer::m_D2DFactory;
ComPtr<ID2D1Device> UITextRenderer::m_D2DDevice;
ComPtr<ID2D1DeviceContext> UITextRenderer::m_D2DContext;
ComPtr<ID2D1Bitmap1> UITextRenderer::m_TargetBitmap;
ComPtr<IDWriteFactory> UITextRenderer::m_DWriteFactory;
bool UITextRenderer::m_IsDrawing = false;

bool UITextRenderer::Init()
{
	D2D1_FACTORY_OPTIONS options{};
	HRESULT hr = D2D1CreateFactory(
		D2D1_FACTORY_TYPE_SINGLE_THREADED,
		__uuidof(ID2D1Factory1),
		&options,
		reinterpret_cast<void**>(m_D2DFactory.GetAddressOf()));
	if (FAILED(hr)) return false;

	ComPtr<IDXGIDevice> dxgiDevice;
	hr = Renderer::GetDevice()->QueryInterface(
		__uuidof(IDXGIDevice),
		reinterpret_cast<void**>(dxgiDevice.GetAddressOf()));
	if (FAILED(hr)) return false;

	hr = m_D2DFactory->CreateDevice(dxgiDevice.Get(), &m_D2DDevice);
	if (FAILED(hr)) return false;

	hr = m_D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_D2DContext);
	if (FAILED(hr)) return false;

	ComPtr<IDXGISurface> dxgiBackBuffer;
	hr = Renderer::GetSwapChain()->GetBuffer(
		0,
		__uuidof(IDXGISurface),
		reinterpret_cast<void**>(dxgiBackBuffer.GetAddressOf()));
	if (FAILED(hr)) return false;

	D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
		D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));

	hr = m_D2DContext->CreateBitmapFromDxgiSurface(
		dxgiBackBuffer.Get(),
		&bitmapProperties,
		&m_TargetBitmap);
	if (FAILED(hr)) return false;

	m_D2DContext->SetTarget(m_TargetBitmap.Get());
	m_D2DContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

	hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(m_DWriteFactory.GetAddressOf()));
	return SUCCEEDED(hr);
}

void UITextRenderer::Uninit()
{
	m_TargetBitmap.Reset();
	m_D2DContext.Reset();
	m_D2DDevice.Reset();
	m_D2DFactory.Reset();
	m_DWriteFactory.Reset();
	m_IsDrawing = false;
}

void UITextRenderer::Begin()
{
	if (!m_D2DContext || m_IsDrawing) return;
	m_D2DContext->SetTransform(D2D1::Matrix3x2F::Identity());
	m_D2DContext->BeginDraw();
	m_IsDrawing = true;
}

void UITextRenderer::End()
{
	if (!m_D2DContext || !m_IsDrawing) return;
	m_D2DContext->EndDraw();
	m_IsDrawing = false;
}

void UITextRenderer::DrawTextUtf8(
	const std::string& text,
	float x,
	float y,
	float width,
	float height,
	float fontSize,
	const D2D1_COLOR_F& color)
{
	DrawTextUtf8Aligned(
		text,
		x,
		y,
		width,
		height,
		fontSize,
		color,
		DWRITE_TEXT_ALIGNMENT_LEADING,
		DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
}

void UITextRenderer::DrawTextUtf8Aligned(
	const std::string& text,
	float x,
	float y,
	float width,
	float height,
	float fontSize,
	const D2D1_COLOR_F& color,
	DWRITE_TEXT_ALIGNMENT textAlignment,
	DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment)
{
	if (!m_D2DContext || !m_DWriteFactory || text.empty()) return;

	std::wstring wideText = ToWide(text);
	if (wideText.empty()) return;

	ComPtr<IDWriteTextFormat> format;
	HRESULT hr = m_DWriteFactory->CreateTextFormat(
		L"Meiryo",
		nullptr,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		fontSize,
		L"ja-jp",
		&format);
	if (FAILED(hr)) return;

	format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
	format->SetTextAlignment(textAlignment);
	format->SetParagraphAlignment(paragraphAlignment);

	ComPtr<ID2D1SolidColorBrush> brush;
	hr = m_D2DContext->CreateSolidColorBrush(color, &brush);
	if (FAILED(hr)) return;

	D2D1_RECT_F rect = D2D1::RectF(x, y, x + width, y + height);
	m_D2DContext->DrawText(
		wideText.c_str(),
		static_cast<UINT32>(wideText.length()),
		format.Get(),
		rect,
		brush.Get());
}

void UITextRenderer::DrawOutlinedTextUtf8(
	const std::string& text,
	float x,
	float y,
	float width,
	float height,
	float fontSize,
	const D2D1_COLOR_F& color)
{
	DrawOutlinedTextUtf8Aligned(
		text,
		x,
		y,
		width,
		height,
		fontSize,
		color,
		DWRITE_TEXT_ALIGNMENT_LEADING,
		DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
}

void UITextRenderer::DrawOutlinedTextUtf8Aligned(
	const std::string& text,
	float x,
	float y,
	float width,
	float height,
	float fontSize,
	const D2D1_COLOR_F& color,
	DWRITE_TEXT_ALIGNMENT textAlignment,
	DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment)
{
	const D2D1_COLOR_F outline = D2D1::ColorF(0.0f, 0.0f, 0.0f, color.a);
	const float offset = 1.5f;

	DrawTextUtf8Aligned(text, x - offset, y, width, height, fontSize, outline, textAlignment, paragraphAlignment);
	DrawTextUtf8Aligned(text, x + offset, y, width, height, fontSize, outline, textAlignment, paragraphAlignment);
	DrawTextUtf8Aligned(text, x, y - offset, width, height, fontSize, outline, textAlignment, paragraphAlignment);
	DrawTextUtf8Aligned(text, x, y + offset, width, height, fontSize, outline, textAlignment, paragraphAlignment);
	DrawTextUtf8Aligned(text, x, y, width, height, fontSize, color, textAlignment, paragraphAlignment);
}

std::wstring UITextRenderer::ToWide(const std::string& text)
{
	int required = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
	if (required <= 0) return std::wstring();

	std::wstring wideText(required - 1, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wideText[0], required);
	return wideText;
}
