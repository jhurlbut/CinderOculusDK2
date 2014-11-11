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

#include "cinder/app/AppNative.h"
#include "cinder/app/RendererGl.h"
#include "cinder/Camera.h"
#include "cinder/gl/Shader.h"
#include "cinder/gl/Fbo.h"
#include "OculusCinder.h"

#include "cinder/gl/Batch.h"
#include "cinder/params/Params.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class OculusBlockApp : public AppNative {
public:
	virtual void	setup();
	virtual void	update();
	virtual void	draw();
	
	void keyDown(KeyEvent event);
	void			prepareSettings(Settings* settings);

	RiftRef				mRift;
	bool				mRiftSwap;

	params::InterfaceGlRef	mParams;
	
	gl::TextureCubeMapRef	mCubeMap;
	gl::BatchRef			mTeapotBatch, mSkyBoxBatch;
	mat4				mObjectRotation;

};

const int SKY_BOX_SIZE = 500;

void OculusBlockApp::prepareSettings(Settings* settings)
{
	settings->disableFrameRate();
	mRift = Rift::create(true);
	//mRift->disableCaps(ovrHmdCap_ExtendDesktop);
	settings->setWindowSize(mRift->getHMDRes());
	settings->setWindowPos(mRift->getHMDDesktopPos()+ivec2(10,10));
	console() << "native res " << mRift->getHMDDesktopPos() << endl;
}


void OculusBlockApp::setup()
{
	mRift->attachToMonitor(app::getWindow()->getNative());
	mRift->enableCaps(ovrHmdCap_LowPersistence);
	mRift->enableCaps(ovrHmdCap_DynamicPrediction);
	
	mRift->initGL();
	//auto displays = Display::getDisplays();
	//app::WindowRef newWindow = createWindow(Window::Format().size(mRift->hmdNativeResolution.x, mRift->hmdNativeResolution.y).display(displays[displays.size() - 1]).fullScreen(true));
	//.pos(mRift->hmdDesktopPosition)
	std::static_pointer_cast<RendererGl>(getRenderer())->setFinishDrawFn([&](cinder::app::Renderer *renderer){
		if (!mRiftSwap)
			renderer->swapBuffers();
	});
	
	//setup rift. let rift swap buffers
	mRiftSwap = true;
	mCubeMap = gl::TextureCubeMap::createHorizontalCross(loadImage(loadAsset("env_map.jpg")), gl::TextureCubeMap::Format().mipmap());

#if defined( CINDER_GL_ES )
	auto envMapGlsl = gl::GlslProg::create(loadAsset("env_map_es2.vert"), loadAsset("env_map_es2.frag"));
	auto skyBoxGlsl = gl::GlslProg::create(loadAsset("sky_box_es2.vert"), loadAsset("sky_box_es2.frag"));
#else
	auto envMapGlsl = gl::GlslProg::create(loadAsset("env_map.vert"), loadAsset("env_map.frag"));
	auto skyBoxGlsl = gl::GlslProg::create(loadAsset("sky_box.vert"), loadAsset("sky_box.frag"));
#endif

	mTeapotBatch = gl::Batch::create(geom::Teapot().subdivisions(7), envMapGlsl);
	mTeapotBatch->getGlslProg()->uniform("uCubeMapTex", 0);

	mSkyBoxBatch = gl::Batch::create(geom::Cube(), skyBoxGlsl);
	mSkyBoxBatch->getGlslProg()->uniform("uCubeMapTex", 0);

	gl::enableDepthRead();
	gl::enableDepthWrite();
	gl::enableAlphaBlending();

	mRift->checkHealthSafetyWarningStatus();
}

void OculusBlockApp::update()
{
	
	// rotate the object (teapot) a bit each frame
	mObjectRotation *= rotate(0.0004f, normalize(vec3(0.01f, .1, 0.01f)));
	
	mRift->draw([&](){
		ovrEyeType eye = mRift->getCurrentEye();
		
		// clear out the FBO with blue
		gl::clear(Color(0.25, 0.5f, 1.0f));

		// setup the viewport to match the dimensions of the FBO
		gl::viewport(0, 0, mRift->mEyeFbos[eye]->getWidth(), mRift->mEyeFbos[eye]->getHeight());
		
		mCubeMap->bind();
		gl::pushMatrices();
		//gl::translate(0.f, 0.f, -50.f); 
		//gl::multModelMatrix(mObjectRotation);
		gl::rotate(toRadians(float(ci::app::getElapsedFrames() % 360)));
		gl::translate(0.f, 0.f, -2.f);
		gl::scale(vec3(1));
		mTeapotBatch->draw();

		gl::rotate(toRadians(-float(ci::app::getElapsedFrames() % 360)));

		gl::translate(-5.f, 0.f, 1.f);
		gl::rotate(toRadians(float(ci::app::getElapsedFrames() % 360)));
		//gl::scale(vec3(3));
		mTeapotBatch->draw();
		gl::popMatrices();

		// draw sky box
		gl::pushMatrices();
		gl::scale(SKY_BOX_SIZE, SKY_BOX_SIZE, SKY_BOX_SIZE);
		mSkyBoxBatch->draw();

		gl::popMatrices();
		
		gl::popModelMatrix();
	});
	 
}
void OculusBlockApp::keyDown(KeyEvent event)
{
	
	
	if (event.getChar() == ' '){
		mRiftSwap = !mRiftSwap;
	}
	if (event.getChar() == KeyEvent::KEY_ESCAPE){
		quit();
	}
}

void OculusBlockApp::draw()
{


}

CINDER_APP_NATIVE(OculusBlockApp, RendererGl)