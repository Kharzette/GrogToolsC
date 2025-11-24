#define NOB_IMPLEMENTATION
#define	NOB_EXPERIMENTAL_DELETE_OLD
#include	"GrogLibsC/nob.h"
#include	<stdlib.h>


//static forward decs
static bool sBuildProgram(const char *szTestFile, bool bReBuild);
static int	sNeedsBuild(const char *szPath, bool bCpp);
static bool	sbIsC(const char *szFileName);
static bool	sbIsCpp(const char *szFileName);


//possible command line args:
//--tools-all	build all tools
//--rebuild		rebuild even if not needed
//blank			build everything
int	main(int argc, char **argv)
{
	//rebuild this program if need be
	NOB_GO_REBUILD_URSELF(argc, argv);

	//check command line args here?
	bool	bToolsAll	=false;
	bool	bReBuild	=false;

	if(argc > 1)
	{
		for(int i=0;i < argc;i++)
		{
			if(strncmp("--tools-all", argv[i], 10) == 0)
			{
				bToolsAll	=true;
			}
			else if(strncmp("--rebuild", argv[i], 9) == 0)
			{
				bReBuild	=true;
				bToolsAll	=true;
			}
		}
	}
	else
	{
		bToolsAll	=true;
	}

	if(bToolsAll)
	{
		if(!sBuildProgram("MeshConvert", bReBuild))
		{
			return	EXIT_FAILURE;
		}
	}

	return	EXIT_SUCCESS;
}


//statics
static void	sStandardProgJunk_Add(Nob_Cmd *pCmd)
{
	nob_cmd_append(pCmd, "gcc", "-g", "-O0", "-march=native",
		"-DCGLM_FORCE_DEPTH_ZERO_TO_ONE",
		"-DCGLM_FORCE_LEFT_HANDED",
		"-I../GrogLibsC/uthash/src",
		"-I../GrogLibsC/dxvk-native/include/native/directx",
		"-I../GrogLibsC/dxvk-native/include/native/windows",
		"-I../GrogLibsC/cglm/include",
		"-I../GrogLibsC",
		"-I../nappgui_src/src",
		"-I../nappgui_src/src/osapp",
		"-I../nappgui_src/src/gui",
		"-Wall", "-fsanitize=address",
		"-Wl,-rpath=libs,--disable-new-dtags");
}

static bool	sbIsC(const char *szFileName)
{
	size_t	len	=strlen(szFileName);
	if(len < 3)
	{
		return	false;
	}

	return	(szFileName[len - 2] == '.'
		&& szFileName[len - 1] == 'c');
}

static bool	sbIsCpp(const char *szFileName)
{
	size_t	len	=strlen(szFileName);
	if(len < 5)
	{
		return	false;
	}

	return	(szFileName[len - 4] == '.'
		&& szFileName[len - 3] == 'c'
		&& szFileName[len - 2] == 'p'
		&& szFileName[len - 1] == 'p');
}

//returns < 0 if error, 0 if no build needed
static int sNeedsBuild(const char *szPath, bool bCpp)
{
	//cd to dir
	if(!nob_set_current_dir(szPath))
	{
		printf("Something went wrong changing to lib dir %s\n", szPath);
		return	false;
	}

	//grab all the source files
    Nob_File_Paths	cFiles		={0};
	Nob_File_Paths	allFiles	={0};
	if(!nob_read_entire_dir(".", &allFiles))
	{
		printf("Something went wrong reading dir %s\n", szPath);
		return	false;
	}

	//add all the source files
	for(int i=0;i < allFiles.count;i++)
	{
		bool	bIsSource;
		if(bCpp)
		{
			bIsSource	=sbIsCpp(allFiles.items[i]);
		}
		else
		{
			bIsSource	=sbIsC(allFiles.items[i]);
		}

		if(bIsSource)
		{
			nob_da_append(&cFiles, allFiles.items[i]);
		}
	}

	nob_da_free(allFiles);

	int	buildNeeded	=nob_needs_rebuild(szPath, cFiles.items, cFiles.count);

	nob_da_free(cFiles);
	
	//pop back up a dir
	if(!nob_set_current_dir(".."))
	{
		printf("Something went wrong changing to ..\n");
		return	-1;
	}
	return	buildNeeded;
}

//pass in the program name (TestUI etc...)
static bool	sBuildProgram(const char *szFile, bool bReBuild)
{
	if(!bReBuild)
	{
		int	needsBuild	=sNeedsBuild(szFile, false);
		if(needsBuild < 0)
		{
			nob_set_current_dir("..");
			return	false;	//error
		}
		else if(!needsBuild)
		{
			printf("No build needed for %s...\n", szFile);
			nob_set_current_dir("..");
			return	true;	//no build needed
		}
	}

	Nob_Cmd	progCmd	={0};

	//cd to dir
	if(!nob_set_current_dir(szFile))
	{
		printf("Something went wrong changing to test dir\n");
		return	false;
	}

	sStandardProgJunk_Add(&progCmd);

    Nob_File_Paths	cFiles		={0};

	if(!nob_read_entire_dir(".", &cFiles))
	{
		printf("Something went wrong reading dir %s\n", szFile);
		return	false;
	}

	//add all the .c files
	for(int i=0;i < cFiles.count;i++)
	{
		if(sbIsC(cFiles.items[i]))
		{
			nob_cmd_append(&progCmd, cFiles.items[i]);
		}
	}

	//more args
	nob_cmd_append(&progCmd, "-std=gnu23", "-o");

	nob_cmd_append(&progCmd, szFile,
		"-L../GrogLibsC/libs",				//link with stuff in ../GrogLibsC/libs
		"-L../nappgui_src/build/Debug/bin",	//link with built nappgui
		"-lm", "-lvulkan", "-lUtilityLib", "-lPhysicsLib",
		"-lMaterialLib", "-lUILib", "-lMeshLib",
		"-lTerrainLib", "-lInputLib", "-lAudioLib",
		"-lFAudio", "-lSDL3", "-lpng",
		"-lgui", "-lcore", "-lcasino", "-ldraw2d", "-lgeom2d",
		"-losbs", "-losgui", "-lsewer", "-losapp", "-ljson-c",
		"../GrogLibsC/dxvk-native/build/src/dxgi/libdxvk_dxgi.so",
		"../GrogLibsC/dxvk-native/build/src/d3d11/libdxvk_d3d11.so");

	bool	bWorked	=nob_cmd_run(&progCmd);

	//pop back up a dir
	if(!nob_set_current_dir(".."))
	{
		printf("Something went wrong changing to ..\n");
		return	false;
	}

	nob_cmd_free(progCmd);
	
	return	bWorked;
}