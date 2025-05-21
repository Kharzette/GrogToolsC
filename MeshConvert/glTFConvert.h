#pragma once

typedef struct	GLTFFile_t			GLTFFile;
typedef struct	Character_t			Character;
typedef struct	Static_t			Static;
typedef struct	AnimLib_t			AnimLib;
typedef struct	StuffKeeper_t		StuffKeeper;
typedef struct	GraphicsDevice_t	GraphicsDevice;

Character	*GLCV_ExtractChar(GraphicsDevice *pGD, const AnimLib *pALib, const StuffKeeper *pSK, const GLTFFile *pGF, bool bVColorIdx);
Static		*GLCV_ExtractStatic(GraphicsDevice *pGD, const StuffKeeper *pSK, const GLTFFile *pGF, bool bVColorIdx);
void		GLCV_ExtractAndAddAnimation(const GLTFFile *pGF, AnimLib **ppALib);