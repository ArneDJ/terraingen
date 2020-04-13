
class Terrain {
public:
	float amplitude;

public:
	Terrain(size_t sidelen, float patchoffst, float amp) 
	{
		sidelength = sidelen;
		amplitude = amp;
		patchoffset = patchoffst;
		termesh =  gen_patch_grid(sidelen, patchoffst);

	};
	~Terrain(void) 
	{
		if (heightimage.data != nullptr) { delete [] heightimage.data; }
		if (normalimage.data != nullptr) { delete [] normalimage.data; }
	};

	void genheightmap(size_t imageres, float freq);
	void gennormalmap(void);
	void display(void);

private:
	size_t sidelength;
	float patchoffset;
	GLuint heightmap;
	GLuint normalmap;
	struct rawimage heightimage;
	struct rawimage normalimage;
	struct mesh termesh;
};
