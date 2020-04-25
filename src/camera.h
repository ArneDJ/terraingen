class Camera {
public:
	glm::vec3 center;
	glm::vec3 up;
	glm::vec3 eye;
	float FOV;
	float nearclip;
	float farclip;
	float aspectratio;
public:
	Camera(glm::vec3 pos, float fov, float aspect, float near, float far);
	void update(float delta);
	glm::mat4 view(void) const;

private:
	float yaw;
	float pitch;
	float sensitivity;
	float speed;
};
