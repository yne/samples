typedef struct {SceIoDirent ent;SceUID fd;} DIR;
int __lastdir;//yup it's an ugly hack for opendir
#define dirent SceIoDirent
#define opendir(path) (((__lastdir=sceIoDopen(path))<0)?NULL:&(DIR){.fd=__lastdir})
#define closedir(dirp)
#define readdir(dirp) (sceIoDread(dirp->fd, &dirp->ent)<=0?NULL:&dirp->ent)
