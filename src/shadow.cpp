#include <iostream>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"
#include "shadow.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define SHADOWMAP_TEXTURE_SIZE 2048

static const glm::mat4 SCALE_BIAS = glm::mat4(glm::vec4(0.5f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 0.5f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 0.5f, 0.0f), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));

struct depthmap gen_depthmap(GLsizei size)
{
	struct depthmap depth;
	depth.height = size;
	depth.width = size;

	// create the depth texture
	glGenTextures(1, &depth.texture);
	glBindTexture(GL_TEXTURE_2D_ARRAY, depth.texture);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, size, size, SHADOW_MAP_CASCADE_COUNT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	//glTexImage2D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	//const float bordercolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	//glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bordercolor);

	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	// create FBO and attach depth texture to it
	glGenFramebuffers(1, &depth.FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, depth.FBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth.texture, 0);
	glDrawBuffer(GL_NONE);

	GLuint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if(status != GL_FRAMEBUFFER_COMPLETE){
	std::cout<< "Framebuffer Error: " << std::hex << status << std::endl;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return depth;
}

Shadow::Shadow(glm::mat4 view, float near, float far)
{ 
	lightview = view; 
	scalebias = SCALE_BIAS;

	depth = gen_depthmap(SHADOWMAP_TEXTURE_SIZE);
}

void Shadow::enable(void) const
{
	glCullFace(GL_FRONT);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(2.0f, 4.0f);

	glBindFramebuffer(GL_FRAMEBUFFER, depth.FBO);

	glViewport(0, 0, depth.width, depth.height);
}

void Shadow::disable(void) const
{
	glDisable(GL_POLYGON_OFFSET_FILL);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glCullFace(GL_BACK);
}

void Shadow::orthoprojection(const Camera *cam)
{
	const float cascadeSplitLambda = 0.9f;
	float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

	float nearClip = 0.1f;
	float farClip = 800.f;
	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	// Calculate split depths based on view camera furstum
	// Based on method presentd in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = cascadeSplitLambda * (log - uniform) + uniform;
		cascadeSplits[i] = (d - nearClip) / clipRange;
	}

	// Calculate orthographic projection matrix for each cascade
	float lastSplitDist = 0.0;
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float splitDist = cascadeSplits[i];

		glm::vec3 frustumCorners[8] = {
		glm::vec3(-1.0f,  1.0f, -1.0f),
		glm::vec3( 1.0f,  1.0f, -1.0f),
		glm::vec3( 1.0f, -1.0f, -1.0f),
		glm::vec3(-1.0f, -1.0f, -1.0f),
		glm::vec3(-1.0f,  1.0f,  1.0f),
		glm::vec3( 1.0f,  1.0f,  1.0f),
		glm::vec3( 1.0f, -1.0f,  1.0f),
		glm::vec3(-1.0f, -1.0f,  1.0f),
		};

		const float aspect = 1920.f/1080.f;
		const glm::mat4 camera_perspective = glm::perspective(glm::radians(90.f), aspect, 0.1f, 800.f);
		// Project frustum corners into world space
		glm::mat4 invCam = glm::inverse(camera_perspective * cam->view());
		for (uint32_t i = 0; i < 8; i++) {
			glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
			frustumCorners[i] = invCorner / invCorner.w;
		}

		for (uint32_t i = 0; i < 4; i++) {
			glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
			frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
			frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
		}

		// Get frustum center
		glm::vec3 frustumCenter = glm::vec3(0.0f);
		for (uint32_t i = 0; i < 8; i++) {
			frustumCenter += frustumCorners[i];
		}
		frustumCenter /= 8.0f;

		float radius = 0.0f;
		for (uint32_t i = 0; i < 8; i++) {
			float distance = glm::length(frustumCorners[i] - frustumCenter);
			radius = max(radius, distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f;

		glm::vec3 maxExtents = glm::vec3(radius);
		glm::vec3 minExtents = -maxExtents;

		const glm::vec3 lightPos = glm::vec3(0.5f, 0.5f, 0.5f);
		glm::vec3 lightDir = normalize(-lightPos);
		glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

		// Store split distance and matrix in cascade
		splitdepth[i] = (0.1f + splitDist * clipRange);
		shadowspace[i] = lightOrthoMatrix * lightViewMatrix;

		lastSplitDist = cascadeSplits[i];
	}
}

void Shadow::update(const Camera *cam, float fov, float aspect, float near, float far)
{
	orthoprojection(cam);
}

void Shadow::bindtextures(void) const 
{
	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D_ARRAY, depth.texture);
}

void Shadow::binddepth(unsigned int section) const
{
	// make the current depth map a rendering target
	glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth.texture, 0, section);

	glClearDepth(1.0f);
	glClear(GL_DEPTH_BUFFER_BIT);
}
