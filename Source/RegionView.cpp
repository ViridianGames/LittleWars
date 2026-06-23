#include "RegionView.h"

#include <cmath>

#include "../Geist/Source/Engine.h"
#include "../Geist/Source/Globals.h"

#include "rlgl.h"

RegionView g_RegionView;

void RegionView::Init()
{
    m_FieldOfView = g_Engine->m_EngineConfig.GetNumber("field_of_view");

    m_Zoom = g_Engine->m_EngineConfig.GetNumber("camera_default_zoom");
    if (m_Zoom <= 0.0f)
    {
        m_Zoom = g_Engine->m_EngineConfig.GetNumber("camera_close_limit");
    }

    m_Camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
    m_Camera.projection = CAMERA_PERSPECTIVE;
}

void RegionView::SyncCamera()
{
    const float offsetX = std::sinf(m_Angle) * m_Zoom;
    const float offsetZ = std::cosf(m_Angle) * m_Zoom;
    const float cameraHeight = m_Zoom * 0.65f;

    m_Camera.position = Vector3{
        m_LookAtX + offsetX,
        cameraHeight,
        m_LookAtZ + offsetZ
    };
    m_Camera.target = Vector3{ m_LookAtX, m_LookAtY, m_LookAtZ };
    m_Camera.fovy = m_FieldOfView;
}

void RegionView::Update(const RegionHeightfield* heightfield)
{
    (void)heightfield;

    const float rotateSpeed = 1.5f;
    const float minZoom = g_Engine->m_EngineConfig.GetNumber("camera_close_limit");
    const float maxZoom = g_Engine->m_EngineConfig.GetNumber("camera_far_limit");
    const float zoomSpeed = g_Engine->m_EngineConfig.GetNumber("camera_zoom_speed");
    const float panTopSpeed = g_Engine->m_EngineConfig.GetNumber("camera_pan_topspeed");
    const float frameScale = GetFrameTime();

    if (IsKeyDown(KEY_Q))
    {
        m_Angle -= rotateSpeed * frameScale;
    }
    if (IsKeyDown(KEY_E))
    {
        m_Angle += rotateSpeed * frameScale;
    }

    if (IsKeyDown(KEY_W))
    {
        m_Zoom -= zoomSpeed * frameScale * 80.0f;
    }
    if (IsKeyDown(KEY_S))
    {
        m_Zoom += zoomSpeed * frameScale * 80.0f;
    }

    if (m_Zoom < minZoom)
    {
        m_Zoom = minZoom;
    }
    if (maxZoom > 0.0f && m_Zoom > maxZoom)
    {
        m_Zoom = maxZoom;
    }

    // Screen-space pan on the XZ plane: right/forward derived from camera orbit angle.
    const float screenRightX = std::cosf(m_Angle);
    const float screenRightZ = -std::sinf(m_Angle);
    const float screenForwardX = -std::sinf(m_Angle);
    const float screenForwardZ = -std::cosf(m_Angle);
    const float panSpeed = panTopSpeed * frameScale * 1.4f * (m_Zoom / minZoom);

    float panX = 0.0f;
    float panZ = 0.0f;
    if (IsKeyDown(KEY_LEFT))
    {
        panX += screenRightX;
        panZ += screenRightZ;
    }
    if (IsKeyDown(KEY_RIGHT))
    {
        panX -= screenRightX;
        panZ -= screenRightZ;
    }
    if (IsKeyDown(KEY_UP))
    {
        panX += screenForwardX;
        panZ += screenForwardZ;
    }
    if (IsKeyDown(KEY_DOWN))
    {
        panX -= screenForwardX;
        panZ -= screenForwardZ;
    }

    m_LookAtX += panX * panSpeed;
    m_LookAtZ += panZ * panSpeed;

    const float minBound = 0.0f;
    const float maxBound = static_cast<float>(REGION_CELLS);
    if (m_LookAtX < minBound)
    {
        m_LookAtX = minBound;
    }
    if (m_LookAtX > maxBound)
    {
        m_LookAtX = maxBound;
    }
    if (m_LookAtZ < minBound)
    {
        m_LookAtZ = minBound;
    }
    if (m_LookAtZ > maxBound)
    {
        m_LookAtZ = maxBound;
    }

    SyncCamera();
}

void RegionView::Begin3D()
{
    BeginMode3D(m_Camera);
    rlDisableBackfaceCulling();
}

void RegionView::End3D()
{
    rlEnableBackfaceCulling();
    EndMode3D();
}