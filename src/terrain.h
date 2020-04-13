
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
		if (heightimage != nullptr) { delete [] heightimage; }
		if (normalimage != nullptr) { delete [] normalimage; }
	};

	void genheightmap(size_t imageres, float freq);
	void display(void);

private:
	size_t sidelength;
	float patchoffset;
	GLuint heightmap;
	GLuint normalmap;
	unsigned char *heightimage = nullptr;
	unsigned char *normalimage = nullptr;
	struct mesh termesh;
};
