//***************************************************************************************
// Camera.h by Frank Luna (C) 2011 All Rights Reserved.
//   
// Simple first person style camera class that lets the viewer explore the 3D scene.
//   -It keeps track of the camera coordinate system relative to the world space
//    so that the view matrix can be constructed.  
//   -It keeps track of the viewing frustum of the camera so that the projection
//    matrix can be obtained.
//***************************************************************************************

#ifndef CAMERA_H
#define CAMERA_H

#include "d3dUtil.h"

class Camera
{
public:

	Camera();
	~Camera();
	/*******************************************/
	// Get/Set world camera position.
	/*******************************************/
	
	// Get world camera position in Vector.
	DirectX::XMVECTOR GetPosition()const;
	// Get world camera position in Float3.
	DirectX::XMFLOAT3 GetPosition3f()const;
	// Set World Camera Position in x,y,z.
	void SetPosition(float x, float y, float z);
	// Set World Camera Position in Float3.
	void SetPosition(const DirectX::XMFLOAT3& v);
	
	/*******************************************/
	// Get camera basis vectors.
	/*******************************************/
	
	// Get View Right Vector in Vector.
	DirectX::XMVECTOR GetRight()const;
	// Get View Right Vector in Float3.
	DirectX::XMFLOAT3 GetRight3f()const;
	// Get View Up Vector in Vector.
	DirectX::XMVECTOR GetUp()const;
	// Get View Up Vector in Float3.
	DirectX::XMFLOAT3 GetUp3f()const;
	// Get View Look Vector in Vector.
	DirectX::XMVECTOR GetLook()const;
	// Get View Look Vector in Float3.
	DirectX::XMFLOAT3 GetLook3f()const;

	/*******************************************/
	// Get frustum properties.
	/*******************************************/
	
	// Get frustum near z value.
	float GetNearZ()const;
	// Get frustum far z value.
	float GetFarZ()const;
	// Get aspect ratio value.
	float GetAspect()const;
	// Get Field of view Y value.
	float GetFovY()const;
	// Get Field of view X value.
	float GetFovX()const;


	/*******************************************/
	// Get near and far plane dimensions in view space coordinates.
	/*******************************************/
	
	float GetNearWindowWidth()const;
	float GetNearWindowHeight()const;
	float GetFarWindowWidth()const;
	float GetFarWindowHeight()const;
	
	// Set frustum.
	void SetLens(float fovY, float aspect, float zn, float zf);

	// Define camera space via LookAt parameters.
	void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
	void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);

	// Get View/Proj matrices.
	DirectX::XMMATRIX GetView()const;
	DirectX::XMMATRIX GetProj()const;

	DirectX::XMFLOAT4X4 GetView4x4f()const;
	DirectX::XMFLOAT4X4 GetProj4x4f()const;

	// Strafe/Walk the camera a distance d.
	void Strafe(float d);
	void Walk(float d);

	// Rotate the camera.
	void Pitch(float angle);
	void RotateY(float angle);

	// After modifying camera position/orientation, call to rebuild the view matrix.
	void UpdateViewMatrix();

private:

	// Camera coordinate system with coordinates relative to world space.
	DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };

	// Cache frustum properties.
	float mNearZ = 0.0f;
	float mFarZ = 0.0f;
	float mAspect = 0.0f;
	float mFovY = 0.0f;
	float mNearWindowHeight = 0.0f;
	float mFarWindowHeight = 0.0f;

	bool mViewDirty = true;

	// Cache View/Proj matrices.
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();
};

#endif // CAMERA_H