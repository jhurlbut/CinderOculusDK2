#pragma once

#include "cinder/Channel.h"
#include "cinder/Matrix.h"
#include "cinder/Thread.h"
#include "cinder/Vector.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/gl.h"
#include <functional>
#include <../Src/OVR_CAPI.h>
#include <../Src/OVR_CAPI_GL.h>
#include <../Src/OVR_Stereo.h>
typedef std::shared_ptr<class Rift> RiftRef;

class Rift
{
public:
	typedef std::function<void()>		RenderFn;
	//! Creates and returns Rift instance.
	static RiftRef	create(bool timeWarpEnable = false);
	void draw(RenderFn renderFunc); 
	void attachToMonitor(void *window);
	void initGL();
	~Rift();

	static void getRiftPositionAndSize(ovrHmd hmd,
		glm::ivec2 & windowPosition, glm::uvec2 & windowSize);
	static glm::quat getStrabismusCorrection();
	static void setStrabismusCorrection(const glm::quat & q);
	static void getHmdInfo(ovrHmd hmd, ovrHmdDesc & ovrHmdInfo);
	static glm::mat4 getMat4(ovrHmd hmd);

	static inline glm::mat4 fromOvr(const ovrFovPort & fovport, float nearPlane = 0.01f, float farPlane = 10000.0f) {
		return fromOvr(ovrMatrix4f_Projection(fovport, nearPlane, farPlane, true));
	}

	static inline glm::mat4 fromOvr(const ovrMatrix4f & om) {
		return glm::transpose(glm::make_mat4(&om.M[0][0]));
	}

	static inline glm::vec3 fromOvr(const ovrVector3f & ov) {
		return glm::make_vec3(&ov.x);
	}

	static inline glm::vec2 fromOvr(const ovrVector2f & ov) {
		return glm::make_vec2(&ov.x);
	}

	static inline glm::uvec2 fromOvr(const ovrSizei & ov) {
		return glm::uvec2(ov.w, ov.h);
	}

	static inline glm::quat fromOvr(const ovrQuatf & oq) {
		return glm::make_quat(&oq.x);
	}

	static inline glm::mat4 fromOvr(const ovrPosef & op) {
		glm::mat4 orientation = glm::mat4_cast(fromOvr(op.Orientation));
		glm::mat4 translation = glm::translate(glm::mat4(), Rift::fromOvr(op.Position));
		return translation * orientation;
		//  return glm::mat4_cast(fromOvr(op.Orientation)) * glm::translate(glm::mat4(), Rift::fromOvr(op.Position));
	}

	static inline ovrMatrix4f toOvr(const glm::mat4 & m) {
		ovrMatrix4f result;
		glm::mat4 transposed(glm::transpose(m));
		memcpy(result.M, &(transposed[0][0]), sizeof(float) * 16);
		return result;
	}

	static inline ovrVector3f toOvr(const glm::vec3 & v) {
		ovrVector3f result;
		result.x = v.x;
		result.y = v.y;
		result.z = v.z;
		return result;
	}

	static inline ovrVector2f toOvr(const glm::vec2 & v) {
		ovrVector2f result;
		result.x = v.x;
		result.y = v.y;
		return result;
	}

	static inline ovrSizei toOvr(const glm::uvec2 & v) {
		ovrSizei result;
		result.w = v.x;
		result.h = v.y;
		return result;
	}

	static inline ovrQuatf toOvr(const glm::quat & q) {
		ovrQuatf result;
		result.x = q.x;
		result.y = q.y;
		result.z = q.z;
		result.w = q.w;
		return result;
	}
	int getEnabledCaps() {
		return ovrHmd_GetEnabledCaps(hmd);
	}
	void enableCaps(int caps) {
		ovrHmd_SetEnabledCaps(hmd, getEnabledCaps() | caps);
	}
	void toggleCap(ovrHmdCaps cap) {
		if (cap & getEnabledCaps()) {
			disableCaps(cap);
		}
		else {
			enableCaps(cap);
		}
	}
	void disableCaps(int caps) {
		ovrHmd_SetEnabledCaps(hmd, getEnabledCaps() &  ~caps);
	}
	inline ovrEyeType getCurrentEye() const {
		return currentEye;
	}
	bool checkHealthSafetyWarningStatus();
	glm::uvec2	getHMDRes(){
		return hmdNativeResolution;
	}
	glm::ivec2 getHMDDesktopPos(){
		return hmdDesktopPosition;
	}
	ci::gl::FboRef			mEyeFbos[2];
	ovrTexture eyeTextures[2];

	glm::uvec2 hmdNativeResolution;
	glm::ivec2 hmdDesktopPosition;
	glm::mat4 getEyePose();
protected:
	Rift(bool timeWarpEnable = false);
	virtual void		update();

	virtual void applyEyePoseAndOffset(const glm::mat4 & eyePose, const glm::vec3 & eyeOffset);

	

	const ovrEyeRenderDesc & getEyeRenderDesc(ovrEyeType eye) const {
		return eyeRenderDescs[eye];
	}

	const ovrFovPort & getFov(ovrEyeType eye) const {
		return eyeRenderDescs[eye].Fov;
	}

	const glm::mat4 & getPerspectiveProjection(ovrEyeType eye) const {
		return projections[eye];
	}

	const glm::mat4 & getOrthographicProjection(ovrEyeType eye) const {
		return orthoProjections[eye];
	}

	const ovrPosef & getEyePose(ovrEyeType eye) const {
		return eyePoses[eye];
	}

	const ovrPosef & getEyePose() const {
		return getEyePose(getCurrentEye());
	}

	const ovrFovPort & getFov() const {
		return getFov(getCurrentEye());
	}

	const ovrEyeRenderDesc & getEyeRenderDesc() const {
		return getEyeRenderDesc(getCurrentEye());
	}

	const glm::mat4 & getPerspectiveProjection() const {
		return getPerspectiveProjection(getCurrentEye());
	}

	const glm::mat4 & getOrthographicProjection() const {
		return getOrthographicProjection(getCurrentEye());
	}
	void	setTimeWarp(bool val){
		mTimeWarpEnabled = val;
	}
	ovrHmd hmd;

	glm::mat4 player;
	ovrPosef  headPose;
	ovrEyeRenderDesc eyeRenderDescs[2];
	glm::mat4 projections[2];
	glm::mat4 orthoProjections[2];
	ovrPosef eyePoses[2];
	ci::gl::TextureRef mTexs[2];
	ovrEyeType currentEye;
	bool		mTimeWarpEnabled;
};

template <typename Function>
void for_each_eye(Function function) {
	for (ovrEyeType eye = ovrEyeType::ovrEye_Left;
		eye < ovrEyeType::ovrEye_Count;
		eye = static_cast<ovrEyeType>(eye + 1)) {
		function(eye);
	}
}