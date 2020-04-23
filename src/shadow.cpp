#include <iostream>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"
#include "shadow.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define NEAR_TEXTURE_SIZE 4096
#define MIDDLE_TEXTURE_SIZE 2048
#define FAR_TEXTURE_SIZE 1024
#define FRUSTUM_DEPTH 800.0f
#define ORTHO_NEAR -500.f
#define ORTHO_FAR 500.f

struct shadowbbox {
	glm::vec3 min;
	glm::vec3 max;
};

struct depthmap gen_depthmap(GLsizei size)
{
 struct depthmap depth;
 depth.height = size;
 depth.width = size;

 // create the depth texture
 glGenTextures(1, &depth.texture);
 glBindTexture(GL_TEXTURE_2D, depth.texture);
 glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

 const float bordercolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
 glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bordercolor);

 glBindTexture(GL_TEXTURE_2D, 0);

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

static const glm::mat4 SCALE_BIAS = glm::mat4(glm::vec4(0.5f, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, 0.5f, 0.0f, 0.0f), glm::vec4(0.0f, 0.0f, 0.5f, 0.0f), glm::vec4(0.5f, 0.5f, 0.5f, 1.0f));

static struct shadowbbox boundaries(glm::vec3 corners[8])
{
	struct shadowbbox box = {
		glm::vec3(1000000.f, 1000000.f, 1000000.f),
		glm::vec3(-1000000.f, -1000000.f, -1000000.f)
	};

	for (int i = 0; i < 8; i++) {
		box.min.x = min(corners[i].x, box.min.x);
		box.max.x = max(corners[i].x, box.max.x);
		box.min.y = min(corners[i].y, box.min.y);
		box.max.y = max(corners[i].y, box.max.y);
		box.min.z = min(corners[i].z, box.min.z);
		box.max.z = max(corners[i].z, box.max.z);
	}

	return box;
}

static inline glm::vec3 viewspace(glm::mat4 view, glm::vec3 position)
{
	glm::vec4 viewpos = view * glm::vec4(position, 1.f);
	return glm::vec3(viewpos.x, viewpos.y, viewpos.z);
}

static void update_split(struct cascade *split, glm::mat4 lightview)
{
	for (int j = 0; j < 8; j++) {
		split->frustumcorners[j] = viewspace(lightview, split->frustumcorners[j]);
	}

	struct shadowbbox bbox = boundaries(split->frustumcorners);

	float left = bbox.min.x;
	float right = bbox.max.x;
	float bottom = bbox.min.y;
	float top = bbox.max.y;
	float near = bbox.min.z;
	float far = bbox.max.z;

	/* TODO fix near and far clipping problem */
	split->orthoproject = glm::ortho(left, right, bottom, top, ORTHO_NEAR, ORTHO_FAR);
}

static void frustum_corners(glm::vec3 view_pos, glm::vec3 view_dir, glm::vec3 view_up, float fov, float aspect, float near, float far, glm::vec3 corners[8])
{
	const glm::vec3 right = glm::cross(view_up, view_dir);
	const float tanFOV = tanf(fov/2.f);

	const float near_height = 2.f * tanFOV * near;
	const float near_width = near_height * aspect;

	const float far_height = 2.f * tanFOV * far;
	const float far_width = far_height * aspect;

	const glm::vec3 near_center = view_pos + view_dir * near;
	const glm::vec3 far_center = view_pos + view_dir * far;

	const glm::vec3 near_topleft = near_center + view_up * (near_height/2.f) - right * (near_width/2.f);
	const glm::vec3 near_topright = near_center + view_up * (near_height/2.f) + right * (near_width/2.f);
	const glm::vec3 near_bottomleft = near_center - view_up * (near_height/2.f) - right * (near_width/2.f);
	const glm::vec3 near_bottomright = near_center - view_up * (near_height/2.f) + right * (near_width/2.f);

	const glm::vec3 far_topleft = far_center + view_up * (far_height/2.f) - right * (far_width/2.f);
	const glm::vec3 far_topright = far_center + view_up * (far_height/2.f) + right * (far_width/2.f);
	const glm::vec3 far_bottomleft = far_center - view_up * (far_height/2.f) - right * (far_width/2.f);
	const glm::vec3 far_bottomright = far_center - view_up * (far_height/2.f) + right * (far_width/2.f);

	corners[0] = near_topleft;
	corners[1] = near_topright;
	corners[2] = near_bottomleft;
	corners[3] = near_bottomright;

	corners[4] = far_topleft;
	corners[5] = far_topright;
	corners[6] = far_bottomleft;
	corners[7] = far_bottomright;
}

Shadow::Shadow(glm::mat4 view, float near, float far)
{ 
	lightview = view; 
	scalebias = SCALE_BIAS;

	nearby.near = near;
	nearby.far = far * 0.1 + 10.f;
	midway.near = (far * 0.1) - 10.f;
	midway.far =  far * 0.4 + 10.f;
	farthest.near =  (far * 0.4) - 10.f;
	farthest.far =  far + 10.f;

	depth[0] = gen_depthmap(NEAR_TEXTURE_SIZE);
	depth[1] = gen_depthmap(MIDDLE_TEXTURE_SIZE);
	depth[2] = gen_depthmap(FAR_TEXTURE_SIZE);
}

void Shadow::enable(void) const
{
	glCullFace(GL_FRONT);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(2.0f, 4.0f);
}

void Shadow::disable(void) const
{
	glDisable(GL_POLYGON_OFFSET_FILL);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glCullFace(GL_BACK);
}

void Shadow::orthoprojection(void)
{
	update_split(&nearby, lightview);
	shadowspace[0] = nearby.orthoproject * lightview;

	update_split(&midway, lightview);
	shadowspace[1] = midway.orthoproject * lightview;

	update_split(&farthest, lightview);
	shadowspace[2] = farthest.orthoproject * lightview;
}

void Shadow::update(const Camera *cam, float fov, float aspect, float near, float far)
{
	frustum_corners(cam->eye, cam->center, cam->up, fov, aspect, nearby.near, nearby.far, nearby.frustumcorners);
	frustum_corners(cam->eye, cam->center, cam->up, fov, aspect, midway.near, midway.far, midway.frustumcorners);
	frustum_corners(cam->eye, cam->center, cam->up, fov, aspect, farthest.near, farthest.far, farthest.frustumcorners);

	orthoprojection();
}

void Shadow::bindtextures(GLenum near, GLenum middle, GLenum far) const 
{
	glActiveTexture(near);
	glBindTexture(GL_TEXTURE_2D, depth[0].texture);
	glActiveTexture(middle);
	glBindTexture(GL_TEXTURE_2D, depth[1].texture);
	glActiveTexture(far);
	glBindTexture(GL_TEXTURE_2D, depth[2].texture);
}

void Shadow::binddepth(enum splitsection section) const
{
	glBindFramebuffer(GL_FRAMEBUFFER, depth[section].FBO);

	glViewport(0, 0, depth[section].width, depth[section].height);
	glClearDepth(1.0f);
	glClear(GL_DEPTH_BUFFER_BIT);

}
