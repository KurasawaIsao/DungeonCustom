#include "ConfirmWindow.h"
#include "main.h" 
#include "input.h"

void ConfirmWindow::Init() {

    //ウィンドウ描画
    m_Window = new Polygon2D();

    m_Window->Init(500, 300, 300, 70, "Asset\\Texture\\StairWindow.png");
    m_Window->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    m_Window->SetScale(Vector3(1.0f, 1.0f, 1.0f));


    m_Window->SetColor(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

    //カーソル描画
    m_Arrow = new Polygon2D();

    m_Arrow->Init(0, 0, 120, 120, "Asset\\Texture\\Arrow.png");
    m_Arrow->SetPosition(Vector3(510.0f, 317.0f, 0.0f)); 
    m_Arrow->SetScale(Vector3(0.3f, 0.3f, 0.3f));

 
    m_Arrow->SetColor(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

    m_Yes = true;
}

void ConfirmWindow::Update() {
    if (m_Enable)
    {
        if (Input::GetKeyTrigger(VK_LEFT))
        {
            m_Yes = !m_Yes;
        }
        else if (Input::GetKeyTrigger(VK_RIGHT))
        {
            m_Yes = !m_Yes;
        }

        if (m_Yes)
        {
            m_Arrow->SetPosition(Vector3(510.0f, 317.0f, 0.0f)); 
        }
        else
        {
            m_Arrow->SetPosition(Vector3(640.0f, 317.0f, 0.0f)); 
        }
        if (Input::GetKeyTrigger('Z'))
        {
            SetDecided(true);
        }
    }
  
}

void ConfirmWindow::Draw() {

    if (m_Enable)
    {
        m_Window->Draw();
        m_Arrow->Draw();
    }
   
}

void ConfirmWindow::Uninit() {
    if (m_Window) {
        m_Window->Uninit();
        delete m_Window;
        m_Window = nullptr;
    }
}