/*
*
* Copyright (c) 2014, James Hurlbut
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or
* without modification, are permitted provided that the following
* conditions are met:
*
* Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in
* the documentation and/or other materials provided with the
* distribution.
*
* Neither the name of James Hurlbut nor the names of its
* contributors may be used to endorse or promote products
* derived from this software without specific prior written
* permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#include "OculusCinder.h"
#include "cinder/app/AppBasic.h"

using namespace ci;
using namespace ci::app;
using namespace std;

RiftRef Rift::create(bool timeWarpEnable)
{
	return RiftRef(new Rift(timeWarpEnable));
}

Rift::Rift(bool timeWarpEnable) 
{
	mTimeWarpEnabled = timeWarpEnable;
	mDirectMode = ovrHmdCap_ExtendDesktop;
	App::get()->getSignalUpdate().connect(bind(&Rift::update, this));
	ovr_Initialize();
	hmd = ovrHmd_Create(0);
	
	if (NULL == hmd) {
		ovrHmdType defaultHmdType = ovrHmd_DK2;
		hmd = ovrHmd_CreateDebug(defaultHmdType);
		hmdDesktopPosition = glm::ivec2(100, 100);
	}
	else {
		hmdDesktopPosition = glm::ivec2(hmd->WindowsPos.x, hmd->WindowsPos.y);

	}
	hmdNativeResolution = glm::ivec2(hmd->Resolution.w, hmd->Resolution.h);
	
	//initGL();

	

}
void Rift::attachToMonitor(void *window){

	ovrHmd_AttachToWindow(hmd, window, nullptr, nullptr);

}
void Rift::initGL()
{
	//set the window size and position if using extended display
	if (mDirectMode == false){
		setWindowPos(hmdDesktopPosition);
		setWindowSize(hmdNativeResolution.x, hmdNativeResolution.y);
	}
	memset(eyeTextures, 0, 2 * sizeof(ovrGLTexture));
	float eyeHeight = 1.5f;
	player = glm::inverse(glm::lookAt(
		glm::vec3(0, eyeHeight, 4),
		glm::vec3(0, eyeHeight, 0),
		glm::vec3(0, 1, 0)));

	gl::enableDepthRead();
	gl::enableDepthWrite();

	gl::Fbo::Format format;

	//format.samples(1);
	//format.setColorTextureFormat(ci::gl::Texture::Format::)
	for_each_eye([&](ovrEyeType eye){

		ovrSizei eyeTextureSize = ovrHmd_GetFovTextureSize(hmd, eye, hmd->DefaultEyeFov[eye], 1.0f);
		mTexs[eye] = gl::Texture2d::create(loadImage(loadAsset("Tuscany_Undistorted_Right_DK2.png")));
		ovrTextureHeader & eyeTextureHeader = eyeTextures[eye].Header;
		eyeTextureHeader.TextureSize = eyeTextureSize;
		eyeTextureHeader.RenderViewport.Size = eyeTextureSize;
		ci::app::console() << "eyetexsize w " << eyeTextureSize.w << " h " << eyeTextureSize.h << endl;
		//format.setSamples( 4 ); // uncomment this to enable 4x antialiasing
		mEyeFbos[0] = gl::Fbo::create(eyeTextureSize.w, eyeTextureSize.h, format.depthBuffer());
		mEyeFbos[1] = gl::Fbo::create(eyeTextureSize.w, eyeTextureSize.h, format.depthBuffer());


		eyeTextureHeader.API = ovrRenderAPI_OpenGL;
		((ovrGLTextureData&)eyeTextures[eye]).TexId =
			mEyeFbos[eye]->getId();

	});

	ovrGLConfig cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.OGL.Header.API = ovrRenderAPI_OpenGL;
		cfg.OGL.Header.RTSize = Rift::toOvr(glm::uvec2(ci::app::getWindowIndex(0)->getWidth(), ci::app::getWindowIndex(0)->getHeight()));
	cfg.OGL.Header.Multisample = 1;

	void* windowHandle = ci::app::getWindowIndex(0)->getNative();
	int distortionCaps =
		ovrDistortionCap_Chromatic |
		ovrDistortionCap_Vignette |
		ovrDistortionCap_Overdrive;
	ci::app::console() << "timewarp " << mTimeWarpEnabled << endl;
	if (mTimeWarpEnabled){
		distortionCaps = distortionCaps |
			ovrDistortionCap_TimeWarp |
			ovrDistortionCap_HqDistortion;
	}
	int configResult = ovrHmd_ConfigureRendering(hmd, &cfg.Config,
		distortionCaps, hmd->MaxEyeFov, eyeRenderDescs);

	float    orthoDistance = 0.8f; // 2D is 0.8 meter from camera
	for_each_eye([&](ovrEyeType eye){
		const ovrEyeRenderDesc & erd = eyeRenderDescs[eye];
		ovrMatrix4f ovrPerspectiveProjection = ovrMatrix4f_Projection(erd.Fov, 0.01f, 1000000.0f, true);
		projections[eye] = Rift::fromOvr(ovrPerspectiveProjection);
		glm::vec2 orthoScale = glm::vec2(1.0f) / Rift::fromOvr(erd.PixelsPerTanAngleAtCenter);
		//orthoProjections[eye] = Rift::fromOvr(
		//	ovrMatrix4f_OrthoSubProjection(
		//	ovrPerspectiveProjection, Rift::toOvr(orthoScale), orthoDistance, erd.ViewAdjust.x));
	});

	if (!ovrHmd_ConfigureTracking(hmd,
		ovrTrackingCap_Orientation | ovrTrackingCap_Position | ovrTrackingCap_MagYawCorrection, 0)) {
		ci::app::console() << ("Could not attach to sensor device");
	}

	///////////////////////////////////////////////////////////////////////////
	// Initialize OpenGL settings and variables
	// Anti-alias lines (hopefully)
	//glEnable(GL_BLEND);
	//glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	//glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
}
void Rift::update()
{
}

void Rift::draw(RenderFn renderFunc)
{
	
	ovrHmd_BeginFrame(hmd, 0);
	glEnable(GL_DEPTH_TEST);

	glm::mat4 modelMat = ci::gl::getModelMatrix();
	glm::mat4 projMat = ci::gl::getProjectionMatrix();
	for_each_eye([&](ovrEyeType eye){
		eyePoses[eye] = ovrHmd_GetHmdPosePerEye(hmd, eye);
	});
	for (int i = 0; i < 2; ++i) {
		
		ovrEyeType eye = currentEye = hmd->EyeRenderOrder[i];
		auto pos = fromOvr(eyePoses[0].Orientation);
		ci::gl::pushMatrices();
		gl::ScopedFramebuffer fbScp(mEyeFbos[eye]);

		const ovrEyeRenderDesc & erd = eyeRenderDescs[eye];
		// Set up the per-eye projection matrix
		{
			glm::mat4 ovrProj = getPerspectiveProjection();
			ci::gl::setProjectionMatrix(ovrProj);
		}

		// Set up the per-eye modelview matrix
		{
			// Apply the head pose
			glm::mat4 eyePose = Rift::fromOvr(eyePoses[eye]);
			glm::vec3 eyeOffset = Rift::fromOvr(erd.HmdToEyeViewOffset);
			applyEyePoseAndOffset(eyePose, eyeOffset);
		}
		renderFunc();

		ci::gl::popMatrices();
	}
	ovrHmd_EndFrame(hmd, eyePoses, eyeTextures);
}
glm::mat4 Rift::getEyePose(){
	return Rift::fromOvr(eyePoses[getCurrentEye()]);
}
bool Rift::checkHealthSafetyWarningStatus(){
	ovrHSWDisplayState hswState;
	ovrHmd_GetHSWDisplayState(hmd, &hswState);
	if (hswState.Displayed) {
		ovrHmd_DismissHSWDisplay(hmd);
		return true;
	}
	else
		return false;
}
void Rift::applyEyePoseAndOffset(const glm::mat4 & eyePose, const glm::vec3 & eyeOffset) {
	glm::mat4 modelMat = ci::gl::getModelMatrix();
	ci::gl::setViewMatrix(ci::gl::getViewMatrix()*glm::translate(glm::mat4(), eyeOffset));
	ci::gl::setModelMatrix(glm::inverse(eyePose)*modelMat);
	
}

Rift::~Rift()
{
	ovrHmd_Destroy(hmd);
	hmd = nullptr;
}
