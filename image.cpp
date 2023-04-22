#ifdef _MSC_VER
#pragma warning(disable:4700)
#endif

#include <iostream>
using namespace std;
#include <thread>
#include <math.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <imgui.h>
#include <imgui_impl_glut.h>
#include <imgui_impl_opengl2.h>
#include <gl/glut.h>

#include <fstream>
ostream file;

#define args float x,float y,float r,float g,float b

#define gist_space 10
const size_t W=600;
const size_t H=600;
constexpr int I=W/8;

const size_t tex_count=14;
static GLuint textures[tex_count];

size_t cur_texture=1;
float graph[tex_count][256];

float p0=0;
float p5=.5;
float p1=1;
float bright=0;
float r_max;
float g_max;
float b_max;
float r_min;
float g_min;
float b_min;

float replace[256]={};

class image
{
	int m;
	
	int w;
	int h;
	int size;
	float**self;

	inline
	void calculate_range(const size_t h,short&step,short&div)
	{
		if(step!=div)
		{
			div=h/div;
			step*=div;
			div=step-div;
		}
		else
		{
			if(div)div=h-h/div-1;
			step=h;
		}
	}

	static float Y(float x,float y,float r,float g,float b)
	{return r*.299+g*.587+b*.114;}

	static float U(float x,float y,float r,float g,float b)
	{return r*-.168+g*-.332+b*.5;}

	static float V(float x,float y,float r,float g,float b)
	{return r*.5+g*-.419+b*-.0813;}

	static float R(float x,float y,float Y,float U,float V)
	{
		const float t=Y+1.4075*V; 
		if(t<0)return 0.;
		if(t>1)return 1.;
		return t;
	}

	static float G(float x,float y,float Y,float U,float V)
	{
		const float t=Y-.3455*U-0.7169*V;
		if(t<0)return 0.;
		if(t>1)return 1.;
		return t;
	}

	static float B(float x,float y,float Y,float U,float V)
	{
		const float t=Y+1.7790*U;
		if(t<0)return 0.;
		if(t>1)return 1.;
		return t;
	}

	static float H(float x,float y,float r,float g,float b)
	{
		if(r==g==b)return 0;

		if(r>g)
		{
			if(g>=b)
			{
				//M=r;
				//m=b;

				return 60*((g-b)/(r-b));
			}
			else
			{
				if(r>b)
				{
					//M=r;
					//m=g;

					return 60*((g-b)/(r-g))+360;
				}
				else
				{
					//M=b;
					//m=g;

					return 60*((r-g)/(b-g))+240;
				}
			}
		}
		else
		{
			if(r>b)
			{
				//M=g;
				//m=b;

				return 60*((b-r)/(g-b))+120;
			}
			else
			{
				if(g>b)
				{
					//M=g;
					//m=r;

					return 60*((b-r)/(g-r))+120;
				}
				else
				{
					//M=b;
					//m=r;

					return 60*((r-g)/(b-r))+240;
				}
			}
		}


	}

	static float S(float x,float y,float r,float g,float b)
	{
		float m;
		float M;

		if(r>g)
		{
			if(g>b)
			{
				M=r;
				m=b;
			}
			else
			{
				if(r>b)
				{
					M=r;
					m=g;
				}
				else
				{
					M=b;
					m=g;
				}
			}
		}
		else
		{
			if(r>b)
			{
				M=g;
				m=b;
			}
			else
			{
				if(g>b)
				{
					M=g;
					m=r;
				}
				else
				{
					M=b;
					m=r;
				}
			}
		}

		if(M)return 1-m/M;
		else return 0;

		return 0;
	}

	static float W(float x,float y,float r,float g,float b)
	{return r>g?(r>b?r:b):(g>b?g:b);}

	static float r(float x,float y,float H,float S,float V)
	{
		if(!S)return V;
		else
		{
			const int sector=floor(H/60);
			const float frac=H/60-sector;
			const float T=V*(1-S);
			const float P=V*(1-S*frac);
			const float Q=V*(1-S*(1-frac));

			switch(sector)
			{
				case 0:return V;
				case 1:return P;
				case 2:return T;
				case 3:return T;
				case 4:return Q;
				case 5:return V;
			}
		}
	}

	static float g(float x,float y,float H,float S,float V)
	{
		if(!S)return V;
		else
		{
			const int sector=floor(H/60);
			const float frac=H/60-sector;
			const float T=V*(1-S);
			const float P=V*(1-S*frac);
			const float Q=V*(1-S*(1-frac));

			switch(sector)
			{
				case 0:return Q;
				case 1:return V;
				case 2:return V;
				case 3:return P;
				case 4:return T;
				case 5:return T;
			}
		}
	}

	static float b(float x,float y,float H,float S,float V)
	{
		if(!S)return V;
		else
		{
			const int sector=floor(H/60);
			const float frac=H/60-sector;
			const float T=V*(1-S);
			const float P=V*(1-S*frac);
			const float Q=V*(1-S*(1-frac));

			switch(sector)
			{
				case 0:return T;
				case 1:return T;
				case 2:return Q;
				case 3:return V;
				case 4:return V;
				case 5:return P;
			}
		}
	}
public:

	image(const int w,const int h,float**self)
		:w(w),h(h),m(w*3),size(m*h),self(self)
	{}
	image(const image&s)
		:w(s.w),h(s.h),m(s.w*3),size(m*s.h),self(new float*[h])
	{
		(*self)=new float[size];

		for(size_t i=size;i--;)
			(*self)[i]=(*s.self)[i];

		const size_t step=w*3;//*3
		for(int i=0;i<h;i++)
			self[i]=(*self)+i*step;
	}
	image(const char*name)
	{
		float*all=stbi_loadf(name,&w,&h,0,3);
		self=new float*[h];
		const size_t step=w*3;//*3
		for(int i=0;i<h;i++)
			self[i]=all+i*step;

		m=w*3;
		size=m*h;
	}
	//~image(){stbi_image_free(*self);}

	const float brigness()const
	{
		float sum=0;

		for(int i=0;i<size;i++)
			sum+=(*self)[i];

		return sum/size;
	}
	
	void clear(const float c=0)
	{
		for(int i=0;i<size;i++)
			(*self)[i]=c;
	}
	void clear(const float r,const float g,const float b)
	{
		for(int i=0;i<size;i+=3)
		{
			(*self)[i]=r;
			(*self)[i+1]=g;
			(*self)[i+2]=b;
		}
	}

	void change(float(*f)(float,float,float),short step=0,short div=0)
	{
		calculate_range(h,step,div);

		for(size_t y=div;y<step;y++)
		{
			for(size_t x=0;x<m;x+=3)
			{
				float*pixel=self[y]+x;

				const float c=f(x/m,y/h,
				(pixel[0]+
				pixel[1]+
				pixel[2])/3);

				self[y][x]=c;
				self[y][x+1]=c;
				self[y][x+2]=c;
			}
		}
	}

	void change(float(*f)(float,float,float,float,float),short step=0,short div=0)
	{
		const size_t m=w*3;

		calculate_range(h,step,div);

		for(size_t y=div;y<step;y++)
		{
			for(size_t x=0;x<m;x+=3)
			{
				float*pixel=self[y]+x;
				const float c=f(x/m,y/h,pixel[0],pixel[1],pixel[2]);

				pixel[0]=c;
				pixel[1]=c;
				pixel[2]=c;
			}
		}
	}

	typedef float(*f)(float,float,float,float,float);
	void change(f fr,f fg,f fb,short step=0,short div=0)
	{
		const size_t m=w*3;

		calculate_range(h,step,div);

		for(size_t y=div;y<step;y++)
		{
			for(size_t x=0;x<m;x+=3)
			{
				float*pixel=self[y]+x;
				const float r=pixel[0];
				const float g=pixel[1];
				const float b=pixel[2];
				const float xf=float(x)/m;
				const float yf=float(y)/h;

				pixel[0]=fr(xf,yf,r,g,b);
				pixel[1]=fg(xf,yf,r,g,b);
				pixel[2]=fb(xf,yf,r,g,b);
			}
		}
	}

	void change(float rt[256],float gt[256],float bt[256],short step=0,short div=0)
	{
		const size_t m=w*3;

		calculate_range(h,step,div);

		for(size_t y=div;y<step;y++)
		{
			for(size_t x=0;x<m;x+=3)
			{
				float*pixel=self[y]+x;
				const float r=pixel[0];
				const float g=pixel[1];
				const float b=pixel[2];
				const float xf=float(x)/m;
				const float yf=float(y)/h;

				pixel[0]=rt[(int)(pixel[0]*255)];
				pixel[1]=gt[(int)(pixel[1]*255)];
				pixel[2]=bt[(int)(pixel[2]*255)];
			}
		}
	}
	void change(float t[256],short step=0,short div=0)
	{change(t,t,t,step,div);}

	//if c<0 then max else min
	const float min_max(char c,short step=0,short div=0)
	{
		const size_t m=w*3;
		calculate_range(h,step,div);

		float r;

		if(c>0)
		{
			r=0;
			c--;

			for(size_t y=div;y<step;y++)
			{
				for(size_t x=0;x<m;x+=3)
				{
					const float color=(self[y]+x)[c];
					if(color>r)r=color;
				}
			}
		}
		else
		{
			r=1;
			c=(-c)-1;

			for(size_t y=div;y<step;y++)
			{
				for(size_t x=0;x<m;x+=3)
				{
					const float color=(self[y]+x)[c];
					if(color<r)r=color;
				}
			}
		}

		return r;
	}

	inline
	void to_yuv(short step=0,short div=0)
	{change(Y,U,V,step,div);}

	inline
	void from_yuv(short step=0,short div=0)
	{change(R,G,B,step,div);}

	inline
	void to_hsv(short step=0,short div=0)
	{change(H,S,W,step,div);}

	inline
	void from_hsv(short step=0,short div=0)
	{change(r,g,b,step,div);}

	inline
	void bind(unsigned int texture)const
	{
#ifdef __gl_h_
		glBindTexture(GL_TEXTURE_2D,texture);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_FLOAT,*self);
#else
		std::cerr<<"opengl is not connected!\n"<<endl;
#endif
	}

	void counter(float*g,const char c=0,float d=100)
	{
		d/=size;

		(*g)=0;for(unsigned char i=1;i++;)g[i]=0;//clear!!!

		for(size_t y=0;y<h;y++)
		{
			for(size_t x=0;x<m;x+=3)
				g[size_t(self[y][x+c]*255)]+=d;
		}
	}
};

float X1=.2;
float X2=.4;
float X3=.6;
float Y0=0;
float Y1=.2;
float Y2=.4;
float Y3=.6;
float Y4=1;

inline
float f(float X1,float Y1,float X2,float Y2)
{return (Y2-Y1)/(X2-X1);}
inline
float g(float X1,float Y2,float K)
{return Y1-K*X1;}

float k1=f(0,Y0, X1,Y1);
float k2=f(X1,Y1, X2,Y2);
float k3=f(X2,Y2, X3,Y3);
float k4=f(X3,Y3, 1,Y4);

float b1=g(0,Y0,k1);
float b2=g(X1,Y1,k1);
float b3=g(X2,Y2,k1);
float b4=g(X3,Y3,k1);

float line(float x)
{
	if(x<0)return .0;
	if(x<X1)return k1*x+b1; 
	if(x<X2)return k2*x+b2;
	if(x<X3)return k3*x+b3;
	if(x<1)return k4*x+b4;

	return 1.;
}

	image original("test.bmp");
void action(const char b=0)
{
	image rgb=original;
	
	if(b==3)
	{
		rgb.to_yuv();
		rgb.change([](args)
		{
			const float tmp=r+::bright*2-1;
			if(tmp<0)return 0.f;
			if(tmp>1)return .999f;
			return tmp;
		},[](args){return g;},[](args){return b;});
		rgb.from_yuv();
	}
	else if(b==5)
	{
		 k1=f(0,Y0, X1,Y1);
		 k2=f(X1,Y1, X2,Y2);
		 k3=f(X2,Y2, X3,Y3);
		 k4=f(X3,Y3, 1,Y4);

		 b1=g(0,Y0,k1);
		 b2=g(X1,Y1,k1);
		 b3=g(X2,Y2,k1);
		 b4=g(X3,Y3,k1);

		rgb.to_yuv();
		rgb.change([](args){return line(r);},[](args){return (g);},[](args){return (b);});
		rgb.from_yuv();
	}
	rgb.bind(textures[0]);

	image R=rgb;
	R.change([](args){return r;},[](args){return 0.f;},[](args){return 0.f;});
	R.bind(textures[1]);
	R.counter(graph[1],0);

	image G=rgb;
	G.change([](args){return 0.f;},[](args){return g;},[](args){return 0.f;});
	G.bind(textures[2]);
	G.counter(graph[2],1);

	image B=rgb;
	B.change([](args){return 0.f;},[](args){return 0.f;},[](args){return b;});
	B.bind(textures[3]);
	B.counter(graph[3],2);
	
	image yuv=rgb;
	yuv.to_yuv();

	image Y=yuv;
	Y.change([](args){return r;},[](args){return 0.f;},[](args){return 0.f;});
	Y.counter(graph[4],0);
	Y.from_yuv();
	Y.bind(textures[4]);

	image U=yuv;
	U.change([](args){return 0.f;},[](args){return g;},[](args){return 0.f;});
	U.counter(graph[5],1);
	U.from_yuv();
	U.bind(textures[5]);

	image V=yuv;
	V.change([](args){return .0f;},[](args){return .0f;},[](args){return b;});
	V.counter(graph[6],2);
	V.from_yuv();
	V.bind(textures[6]);

	image hsv=rgb;
	hsv.to_hsv();

	image H=hsv;
	H.change([](args){return r/360;},[](args){return 1.f;},[](args){return .5f;});
	H.counter(graph[7],0);
	H.change([](args){return r*360;},[](args){return 1.f;},[](args){return .5f;});
	H.from_hsv();
	H.bind(textures[7]);

	image S=hsv;
	S.change([](args){return g;},[](args){return g;},[](args){return g;});
	S.counter(graph[8],1);
	//S.from_hsv();
	S.bind(textures[8]);

	image W=hsv;
	W.change([](args){return 0.f;},[](args){return 0.f;},[](args){return b;});
	W.counter(graph[9],2);
	W.from_hsv();
	W.bind(textures[9]);

	//autolevels
	image z1=rgb;
	r_min=z1.min_max(-1);
	g_min=z1.min_max(-2);
	b_min=z1.min_max(-3);
	r_max=z1.min_max(1)-r_min;
	g_max=z1.min_max(2)-g_min;
	b_max=z1.min_max(3)-b_min;
	z1.change([](args){return (r-r_min)/r_max;},[](args){return (g-g_min)/g_max;},[](args){return (b-b_min)/b_max;});
	z1.bind(textures[11]);
}

void render()
{
	glClear(GL_COLOR_BUFFER_BIT);
	
	glBindTexture(GL_TEXTURE_2D,0);
	glBegin(GL_LINE_STRIP);
		glColor3f(1,1,1);
		for(unsigned char i=0;i<255;i++)
			glVertex2f(i/255.f,graph[cur_texture][i]);
	glEnd();
	/*glBegin(GL_LINE_STRIP);
		for(unsigned char i=0;i<255;i++)
		{
			const float t=i/255.f;
			glVertex2f((t-1)/2,line(t)/2);
		}
	glEnd();*/
	glBegin(GL_LINE_STRIP);
		glVertex3f(-.5,Y0/2,0);
		glVertex3f((X1-1)/2,Y1/2,0);
		glVertex3f((X2-1)/2,Y2/2,0);
		glVertex3f((X3-1)/2,Y3/2,0);
		glVertex3f(0,Y4/2,0);
	glEnd();
	
	glBindTexture(GL_TEXTURE_2D,textures[0]);
	glBegin(GL_QUADS);
		glTexCoord2f(1,0);	glVertex3f(0,0,0);
		glTexCoord2f(0,0);	glVertex3f(-1,0,0);
		glTexCoord2f(0,1);	glVertex3f(-1,-1,0);
		glTexCoord2f(1,1);	glVertex3f(0,-1,0);
	glEnd();
	
	glBindTexture(GL_TEXTURE_2D,textures[cur_texture]);
	glBegin(GL_QUADS);
		glTexCoord2f(1,0);	glVertex3f(1,0,0);
		glTexCoord2f(0,0);	glVertex3f(0,0,0);
		glTexCoord2f(0,1);	glVertex3f(0,-1,0);
		glTexCoord2f(1,1);	glVertex3f(1,-1,0);
	glEnd();

	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplGLUT_NewFrame();

	ImGui::SetNextWindowPos(ImVec2(0,0));
	ImGui::SetNextWindowSize(ImVec2(W>>1,H>>1-gist_space));

	ImGui::Begin("imgui_window",0,3);

	if(ImGui::Button("R"))cur_texture=1;
	ImGui::SameLine();
	if(ImGui::Button("G"))cur_texture=2;
	ImGui::SameLine();
	if(ImGui::Button("B"))cur_texture=3;
	ImGui::SameLine();
	ImGui::Indent(I);

	if(ImGui::Button("Y"))cur_texture=4;
	ImGui::SameLine();
	if(ImGui::Button("U"))cur_texture=5;
	ImGui::SameLine();
	if(ImGui::Button("W"))cur_texture=6;
	ImGui::SameLine();
	ImGui::Indent(I);

	if(ImGui::Button("H"))cur_texture=7;
	ImGui::SameLine();
	if(ImGui::Button("S"))cur_texture=8;
	ImGui::SameLine();
	if(ImGui::Button("V"))cur_texture=9;
	ImGui::SameLine();
	ImGui::Indent(I);

	if(ImGui::Button("O"))action();//cur_texture=10;
	ImGui::SameLine();
	if(ImGui::Button("1"))cur_texture=11;
	ImGui::SameLine();
	if(ImGui::Button("2"))cur_texture=12;

	ImGui::Separator();

	ImGui::Indent(-I*3);
	ImGui::SliderFloat("bright",&bright,0,1,"%.3f");
	ImGui::SameLine();
	if (ImGui::Button("change!"))action(3);
	/*{
		rgb=store;

		rgb.to_yuv();
		rgb.change([](args)
		{
			const float tmp=r+::bright*2-1;
			if(tmp<0)return 0.f;
			if(tmp>1)return .999f;
			return tmp;
		},[](args){return g;},[](args){return b;});
		rgb.from_yuv();
		rgb.bind(textures[0]);
	}*/

	ImGui::Separator();

	if(ImGui::BeginListBox("points3",ImVec2(W>>1,H/6)))
	{
		ImGui::SliderFloat("X1",&X1,0,1,"%.3f");
		ImGui::SameLine();
		if(ImGui::Button("change!!"))
		{
			action(5);
		}
		ImGui::SliderFloat("X2",&X2,0,1,"%.3f");
		ImGui::SliderFloat("X3",&X3,0,1,"%.3f");
		ImGui::SliderFloat("Y0",&Y0,0,1,"%.3f");
		ImGui::SliderFloat("Y1",&Y1,0,1,"%.3f");
		ImGui::SliderFloat("Y2",&Y2,0,1,"%.3f");
		ImGui::SliderFloat("Y3",&Y3,0,1,"%.3f");
		ImGui::SliderFloat("Y4",&Y4,0,1,"%.3f");


		ImGui::EndListBox();
	}

	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
 
	glutSwapBuffers();
	glutPostRedisplay();
}
inline
void opengl_init()
{
	glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB);
	glutInitWindowPosition(100,100);
	glutInitWindowSize(W,H);
	glutCreateWindow("test");
	glEnable(GL_TEXTURE_2D);
	glGenTextures(tex_count,textures);
	//glLineWidth(4);
}
inline
void config_imgui()
{
	ImGui::StyleColorsLight();
	ImGuiStyle&style=ImGui::GetStyle();
	ImGuiIO&io=ImGui::GetIO();

	style.WindowMenuButtonPosition=ImGuiDir_None;
	constexpr size_t temp=W/100;//65;
	style.FramePadding=ImVec2(temp,temp);
}

int main(int argc,char**argv)
{
	glutInit(&argc,argv);
	opengl_init();

	action();

	glutDisplayFunc(render);
	glutIdleFunc(render);

		ImGui::CreateContext();
		config_imgui();
		ImGui_ImplGLUT_Init();
		ImGui_ImplGLUT_InstallFuncs();
		ImGui_ImplOpenGL2_Init();
		glutReshapeFunc([](int,int){glutReshapeWindow(W,H);ImGui_ImplGLUT_ReshapeFunc(W,H);});
	glutMainLoop();
		ImGui_ImplOpenGL2_Shutdown();
		ImGui_ImplGLUT_Shutdown();
		ImGui::DestroyContext();
	
	return 0;
}
