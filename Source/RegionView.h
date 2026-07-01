#ifndef _REGIONVIEW_H_
#define _REGIONVIEW_H_

#include "GameGlobals.h"
#include "raylib.h"

class RegionView
{
public:
    void Init();
    void Update(const RegionHeightfield* heightfield);
    void Begin3D();
    void End3D();

    const Camera3D& GetCamera() const { return m_Camera; }
    float GetLookAtX() const { return m_LookAtX; }
    float GetLookAtZ() const { return m_LookAtZ; }
    float GetZoom() const { return m_Zoom; }
    float GetAngle() const { return m_Angle; }
    void SetLookAtPosition(float x, float z);

private:
    void SyncCamera();

    Camera3D m_Camera{};
    float m_FieldOfView = 45.0f;
    float m_LookAtX = REGION_CELLS * 0.5f;
    float m_LookAtY = 0.0f;
    float m_LookAtZ = REGION_CELLS * 0.5f;
    float m_Zoom = 26.0f;
    float m_Angle = 0.78539816339f;
};

extern RegionView g_RegionView;

#endif