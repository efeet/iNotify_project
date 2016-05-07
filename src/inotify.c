/**
 * by USC
 * @_eFeet_
 * https://github.com/efeet 
 * Octubre 2015.
 */

#define _GNU_SOURCE
#include "IncludeLibraries.h"
#include "EnumIpInter.h"

#define APP_USAGE "Error de uso: \
                   \nUso: \
                   \n\nIniciar Agente de monitoreo:  -c  inotify.cfg \
                   \nDetener Agente de monitoreo:  -k  inotify.cfg\n"

#define errExit(msg)    do{ \
			logMessage(0, msg); \
			exit(EXIT_FAILURE); \
                        }while (0)

#define VB_BASIC 1      /* Basic messages */
#define VB_NOISY 2      /* Verbose messages */
#define VB_EXTRANOISY 4 /* Puede generar mucho log*/
static int verboseMask; /*Variable para indicar nivel de verbose de log*/

static FILE *fvalues = NULL; /*Variable para abrir archivo de configuracion.*/
static FILE *logfp = NULL; /*Variable para abrir archivo de Log.*/
char ipconsole[256]; /*Variable para la direccion IP de la consola*/
char logpath[PATH_MAX]; /*Variable con la ruta del log*/
int logsizelimit = 0; /*Variable con el taman~o del archivo de log*/
char hostname[256]; /*Variable para el hostname del servidor*/
char allIps[PATH_MAX]; /*Variable para todas las direcciones IP del servidor*/
int justkill = 0; /*Bandera para indicar solo kill al proceso*/
int showchanges = 0; /*Bandera para indicar si muestra los cambios a objetos*/
int modifiedband=0; /*Bandera para indicar cambios en archivo*/
/** Validacion de conexion con el socket
 *  en caso de fallar, el registro de eventos
 *  queda en el archivo log */
int SockConn = 0;

static void logMessage(int vb_mask, const char *format, ...)
{
  va_list argList;
  if ((vb_mask == 0) || (vb_mask & verboseMask)) {
      fprintf(logfp,"%s : ",currTimeLog());
      va_start(argList, format);
      vfprintf(logfp, format, argList);
      va_end(argList);
      fprintf(logfp," \n");
  }
}

/* --- from GPLv2 and Kernel Linux--- */

void *xrealloc(void *ptr, size_t size)
{
        void *ret = realloc(ptr, size);
        if (!ret && !size)
                ret = realloc(ptr, 1);
        if (!ret) {
                ret = realloc(ptr, size);
                if (!ret && !size)
                        ret = realloc(ptr, 1);
                if (!ret){
		    logMessage(0, "Fuera de Memoria, fallo realloc.");
		    exit(EXIT_FAILURE);
		}
        }
        return ret;
}

#define alloc_nr(x) (((x)+16)*3/2)

/*
 * Realloc the buffer pointed at by variable 'x' so that it can hold
 * at least 'nr' entries; the number of entries currently allocated
 * is 'alloc', using the standard growing factor alloc_nr() macro.
 *
 * DO NOT USE any expression with side-effect for 'x', 'nr', or 'alloc'.
 */
#define ALLOC_GROW(x, nr, alloc) \
        do { \
                if ((nr) > alloc) { \
                        if (alloc_nr(alloc) < (nr)) \
                                alloc = (nr); \
                        else \
                                alloc = alloc_nr(alloc); \
                        x = xrealloc((x), alloc * sizeof(*(x))); \
                } \
        } while (0)

/* --- end from GPLv2 and Kernel Linux- --- */

char IGNORE_FILE_NAME[PATH_MAX]; /*Variable con valor de ruta de Objetos a Ignorar*/
char **ignore_patterns = NULL;
int ignore_alloc = 0;

char **wdpaths = NULL;
int wd_alloc = 0;

#define HIST 5
const char *lru[HIST] = {0};

/* Funcion para verificar los permisos de los objetos*/
static void CheckPerm(char fullPathPerm[PATH_MAX])
{
    char sendBuff[PATH_MAX], clearsendBuff[PATH_MAX];//Para Sockects
    int sock_inits = 0, sock_send = 0, sock = 0;
    struct stat buf_stat;

    stat(fullPathPerm, &buf_stat);
    if(buf_stat.st_mode & S_IWOTH){
      //Verificamos el LOG y los rotamos.
      logfp = rotatelog(logpath, logfp, logsizelimit);
      setbuf(logfp, NULL);
      if(SockConn == 1){
	for (sock_inits=1; sock_inits<3; sock_inits++){
	  logMessage(VB_NOISY,"Intento %d de conexion de Socket...",sock_inits);
	  sock = OS_ConnectPort(514,ipconsole);
	  if( sock > 0 ){
	    logMessage(VB_NOISY,"Conexion Exitosa.");
	    break;
	  }
	}
	snprintf(sendBuff, sizeof(sendBuff),"%s|%s|%sWARN|Write Perm for Group Others|%s\r\n",currTime(), hostname, allIps, fullPathPerm); //Construir mensaje
	sock_send = write(sock, sendBuff, strlen(sendBuff)); //Envio a socket.
	if( sock_send < 0 )
	  logMessage(0,"Error al enviar a Socket.");
      }
      logMessage(0,"---->Objeto Con Escritura Publica=%s",fullPathPerm);
      bzero(fullPathPerm,PATH_MAX);
      strcpy(fullPathPerm, clearsendBuff);
      bzero(sendBuff,PATH_MAX);
      strcpy(sendBuff, clearsendBuff);
      OS_CloseSocket(sock);
      OS_CloseSocket(sock_send);
    }
}

void set_dirpath(int wd, const char *path)
{
        int old_alloc = wd_alloc;
        ALLOC_GROW(wdpaths, wd+1, wd_alloc);
        if (old_alloc < wd_alloc)
                memset(wdpaths+old_alloc, 0,
                       (wd_alloc-old_alloc)*sizeof(const char *));
        wdpaths[wd] = strdup(path);
}

void die_errno(const char *msg)
{
        perror(msg);
        exit(EXIT_FAILURE);
}

int ifd = -1;

void read_ignore_file()
{
        FILE *fp;
        int i = 0;
        char *line = NULL;
        size_t len;
        int ret;

        ALLOC_GROW(ignore_patterns, 1, ignore_alloc);
        ignore_patterns[0] = NULL;

	if(!(fp = fopen(IGNORE_FILE_NAME, "r"))){
                if (errno == ENOENT)
                        return;
                die_errno("fopen");
        }

        while ((ret = getline(&line, &len, fp)) > 0) {
                i++;
                ALLOC_GROW(ignore_patterns, i+1, ignore_alloc);
                if (ret != strlen(line)){
		  logMessage(0, "\\0 en Archivo Ignore.");
		  exit(EXIT_FAILURE);
		}
                if (line[ret-1] == '\n')
                        line[ret-1] = '\0';
                ignore_patterns[i-1] = line;
                line = NULL;
        }

        ignore_patterns[i] = NULL;

        if (fclose(fp) != 0)
                die_errno("fclose");
}

int is_ignored(const char *path)
{
        char **pp = ignore_patterns;
        while (*pp) {
                if (!fnmatch(*pp, path, 0))
                        return 1;
                pp++;
        }
        return 0;
}

struct dirent *xreaddir(DIR *dirfd)
{
        struct dirent *ent;
        errno = 0;
        ent = readdir(dirfd);
        if (!ent && errno)
                die_errno("readdir");
        return ent;
}

int isdir(const char *path)
{
        struct stat st;
        if (lstat(path, &st) < 0)
                die_errno("stat");
        return S_ISDIR(st.st_mode);
}

const int MASK =
        IN_ACCESS
        | IN_ATTRIB
        | IN_CREATE
        | IN_MODIFY
        | IN_CLOSE_WRITE
        | IN_MOVE_SELF
        | IN_DELETE_SELF
        | IN_MOVED_TO
        | IN_OPEN
        | IN_DONT_FOLLOW
        //| IN_EXCL_UNLINK
        | IN_UNMOUNT;

const char *event_msg(int mask)
{
        if (mask & IN_ACCESS)
                return "access";
        else if (mask & IN_ATTRIB)
                return "attrib";
        else if (mask & IN_CREATE)
                return "create";
        else if (mask & IN_MODIFY)
                return "modify";
	else if (mask & IN_CLOSE_WRITE)
                return "in_close_write";
        else if (mask & IN_MOVE_SELF)
                return "move_self";
	else if (mask & IN_DELETE_SELF)
                return "in_delete_self";
        else if (mask & IN_MOVED_TO)
                return "moved_to";
        else if (mask & IN_OPEN)
                return "open";
        else if (mask & IN_Q_OVERFLOW)
                return "<overflow>";
        return "<huh?>";
}

int setup_one_watch (const char *dir)
{
        int wd;
        wd = inotify_add_watch(ifd, dir, MASK);
        if (wd < 0) {
	  logMessage(0,"Omitiendo Objeto por error: %s: %s",dir, strerror(errno));
                if (errno == EACCES || errno == ENOENT)
                        return -1;
                die_errno("inotify_add_watch");
        }
        set_dirpath(wd, dir);
        return 0;
}

void setup_watches (const char *dir)
{
        DIR *dirfd = opendir(dir);
        char fullent[PATH_MAX];
        int dirlen = strlen(dir);
        memcpy(fullent, dir, dirlen);
	fullent[dirlen] = '/';
        struct dirent *ent;
        if (!dirfd) {
                if (errno == ENOENT || errno == EACCES)
                        return;
                die_errno("opendir");
        }
        while ((ent = xreaddir(dirfd))) {
                int entlen = strlen(ent->d_name);
                if (!strcmp(ent->d_name, ".")
                    || !strcmp(ent->d_name, ".."))
                        continue;
                memcpy(fullent+dirlen+1, ent->d_name, entlen);
                fullent[dirlen+1+entlen] = '\0';
                if (is_ignored(fullent))
                        continue;
                if (!isdir(fullent))
                        continue;
                if (!setup_one_watch(fullent))
                        setup_watches(fullent);
        }
        if (closedir(dirfd))
                die_errno("closedir");
}

void handle_event(struct inotify_event *ev)
{
    char *wdp;
    char fullPath[PATH_MAX];
    char fullPath2[PATH_MAX], strcompara[PATH_MAX];
    char buf[PATH_MAX];
    int i, j, compara = 0;
    if (ev->wd < 0)
	    return;
    wdp = wdpaths[ev->wd];
    if (!wdp)
	    return;
    if (ev->mask != IN_ACCESS)
	    logMessage(VB_EXTRANOISY,"--%08x-- --%s-- --%s-- --%s--", ev->mask, event_msg(ev->mask), wdp, ev->len ? ev->name : "(none)");

    if (ev->mask & IN_IGNORED) {
	    wdpaths[ev->wd] = NULL;
	    for (i = 0; i < HIST; i++) {
		    if (!lru[i])
			    break;
		    if (strcmp(wdp, lru[i]))
			    continue;
		    for (j = i; j < HIST-1; j++)
			    lru[j] = lru[j+1];
		    lru[HIST-1] = NULL;
		    break;
	    }
	    free(wdp);
	    return;
    }
    if ((ev->mask & IN_ISDIR) && (ev->mask & (IN_CREATE | IN_MOVED_TO))) {
	  strcpy(buf, wdp);
	  strcat(buf, "/");
	  strcat(buf, ev->name);
	  if (is_ignored(buf))
		  return;
	  if (!setup_one_watch(buf))
		  setup_watches(buf);
	  return;
    } else if (ev->mask & IN_Q_OVERFLOW) {
	  logMessage(VB_NOISY, "Sobrecarga en cola..");
    } else if (ev->mask & IN_UNMOUNT) {
	  logMessage(VB_NOISY, "Filesystem Desmontado: %s",wdp);
    } else if (ev->mask & IN_ATTRIB) {
	  if (ev->mask & IN_ISDIR) {
	      strcpy(strcompara, wdp);
	      strcpy(fullPath2, wdp);
	      if(ev->name && ev->len)
		strcat(fullPath2, ev->name);
	      compara = strcmp(fullPath2, strcompara);
	      if(compara == 0){
		    strcpy(fullPath, wdp);
		    CheckPerm(fullPath);
	      }
	    }
	  else {
	    strcpy(buf, wdp);
	    strcat(buf, "/");
	    strcat(buf, ev->name);
	    strcpy(fullPath, buf);
	    CheckPerm(fullPath);
	  } 
    } else if(ev->mask & IN_OPEN && modifiedband == 0){
	modifiedband = 1;
    } else if(ev->mask & IN_MODIFY && modifiedband == 1){
	modifiedband = 2;
    } else if(ev->mask & IN_DELETE || ev->mask & IN_CREATE ){
	modifiedband = 0;
    } else if(ev->mask & IN_CLOSE_WRITE && modifiedband == 2 && showchanges == 1){
	strcpy(buf, wdp);
	strcat(buf, "/");
	strcat(buf, ev->name);
	strcpy(fullPath, buf);
	logMessage(0, "Modificacion de contenido en: %s",fullPath);
	modifiedband = 0;
    }

    if (ev->len && ev->mask & IN_ISDIR)
      return;
    for (i = 0; i < HIST; i++) {
	    if (!lru[i])
		    break;
	    if (strcmp(wdp, lru[i]))
		    continue;
	    if (i == 0)
		    return;
	    for (j = i; j > 0; j--)
		    lru[j] = lru[j-1];
	    lru[0] = wdp;
	    return;
    }
    for (j = HIST-1; j > 0; j--)
	    lru[j] = lru[j-1];
    lru[0] = wdp;
}

static inline int event_len(struct inotify_event *ev)
{
        return sizeof(struct inotify_event) + ev->len;
}

void handle_inotify ()
{
    char buf[4096+PATH_MAX];
    ssize_t ret = read(ifd, &buf, sizeof(buf));
    int handled = 0;
    if (ret == -1) {
	    if (errno == EINTR)
		    return;
	    die_errno("read");
    }
    while (handled < ret) {
	    char *p = buf + handled;
	    struct inotify_event *ev = (struct inotify_event *) p;
	    handle_event(ev);
	    handled += event_len(ev);
    }
}

static int LoadValues(){
  char line[PATH_MAX + 1];
  char *token, *token2;
  char *parameters[] = { "logpath", "pidpath" , "logverbose" , "ipconsole" , "fileignore", "paths", "showchanges", "logsize" }; 
  int max_watches;

  max_watches = chk_kernel();

  while( fgets(line, 1024, fvalues) != NULL ){
    token = strtok(line, "\t =\n\r");
    if(token != NULL && token[0] != '#'){
      if(justkill == 0){
	if(!strncmp(token, parameters[0], sizeof(parameters[0]))){
		token = strtok( NULL, "\t =\n\r");
		strcpy(logpath, token);
		logfp = fopen(token, "a+");
		if (logfp == NULL){
		  perror("Archivo log Error...\n");
		  exit(EXIT_FAILURE);
		}
		setbuf(logfp, NULL);
	}
	if(!strncmp(token, parameters[1], sizeof(parameters[1]))){
		id_t pid = getpid();
		token = strtok( NULL, "\t =\n\r");
		FILE *fpid = fopen(token, "w");
		if (!fpid){
		  perror("Archivo pid Error...\n");
		  exit(EXIT_FAILURE);
		}
		fprintf(fpid, "%d\n", pid);
		fclose(fpid);
	}
	if(!strncmp(token, parameters[2], sizeof(parameters[2]))){
		token = strtok( NULL, "\t =\n\r");
		verboseMask = atoi(token);
		logMessage(VB_BASIC,"Log establecido como Basico...");
		logMessage(VB_NOISY,"Log establecido como Ruidoso...");
		logMessage(VB_EXTRANOISY,"Log establecido como Extra-Ruidoso...");
		logMessage(0, "ATENCION: El numero de directorios soportados es: %d", max_watches);
		logMessage(0, "          para modificar este numero, se edita el parametro de kernel");
		logMessage(0, "          /proc/sys/fs/inotify/max_user_watches");
	}
	if(!strncmp(token, parameters[3], sizeof(parameters[3]))){
		token = strtok( NULL, "\t =\n\r");
		strncpy(ipconsole, token, sizeof(ipconsole)-1 );
		ipconsole[sizeof(ipconsole)-1] = '\0';
	}
	if(!strncmp(token, parameters[4], sizeof(parameters[4]))){
	  token = strtok(NULL, "\n\r");
	  strncpy(IGNORE_FILE_NAME, token, sizeof(IGNORE_FILE_NAME)-1);
	  IGNORE_FILE_NAME[sizeof(IGNORE_FILE_NAME)-1] = '\0';
	  read_ignore_file();
	}
	if(!strncmp(token, parameters[5], sizeof(parameters[5]))){
	  token = strtok(NULL, "\n\r");
	  token2 = strtok(token, "|");
	  while(token2 != NULL){
	    if(!setup_one_watch(token2)){
	      setup_watches(token2);
	    } else { return 0; }
	    token2 = strtok(NULL, "|");
	  }
	}
	if(!strncmp(token, parameters[6], sizeof(parameters[6]))){
	  token = strtok( NULL, "\t =\n\r");
	  showchanges = atoi(token);
	}
	if(!strncmp(token, parameters[7], sizeof(parameters[7]))){
		token = strtok( NULL, "\t =\n\r");
		logsizelimit = atoi(token);
	}
      }
      else {
	int getpid = 0;
	if(!strncmp(token, parameters[1], sizeof(parameters[1]))){
	  token = strtok( NULL, "\t =\n\r");
	  FILE *fpid = fopen(token, "r");
	  if (!fpid){
	    perror("Archivo pid Error\n");
	    exit(EXIT_FAILURE);
	  }
	  fscanf(fpid, "%d", &getpid);
	  if(!kill(getpid, SIGKILL)){
	    fclose(fpid);
	    //printf("Exito al terminar pid.\n");
	    exit(EXIT_SUCCESS);
	  }
	  else{
	    fclose(fpid);
	    //printf("No se pudo terminar pid.\n");
	    exit(EXIT_FAILURE);
	    }
	}
      }
    }
  }
  return 1;
}

static void ReadParameters(int argc, char *argv[]){
  int opt;
  char *p = malloc(strlen(argv[2] + 1));
  char namecfg[11]="inotify.cfg";
  char *token, *token2;

  while ((opt = getopt(argc, argv, "c:k")) != -1) {
    strcpy(p, argv[2]);
    token = strtok(p, "\n\r");
    token2 = strtok(token, "/");
     switch (opt) {
      case 'c': 
	while(token2 != NULL){
	 if(!strncmp(token2, namecfg, sizeof(namecfg))){
	   if(!LoadValues()){
	     printf("Error en la carga de valores en archivo inotify.cfg..\n");
	     exit(EXIT_FAILURE);
	   }
	   break;
	 }
	 else{
	   token2 = strtok(NULL, "/");
	 }
	}
      break;
      case 'k':
	justkill = 1;
	while(token2 != NULL){
	  if(!strncmp(token2, namecfg, sizeof(namecfg))){
	    if(!LoadValues()){
	     printf("Error en la carga de valores en archivo inotify.cfg..\n");
	     exit(EXIT_FAILURE);
	    }
	    break;
	  }
	  else{
	    token2 = strtok(NULL, "/");
	  }
	}
      break;
      default:
	printf(APP_USAGE);
	exit(EXIT_FAILURE);
     }
  }
}


int main (int argc, char *argv[])
{
  fd_set rfds;
  int ret, controla1;
  int maxfd, sock, i;

  if (optind >= argc || argc < 3 || argc > 3 ){
    printf(APP_USAGE);
    exit(EXIT_FAILURE);
  }

  fvalues = fopen(argv[2], "r");
  if( fvalues == NULL ){
    printf("Error ruta incorrecta de Archivo inotify.cfg..\n");
    exit(EXIT_FAILURE);
  }

  ifd = inotify_init();
  if (ifd < 0)
	  die_errno("inotify_init");
  ReadParameters(argc, argv);

  //Validacion de Socket para envio de mensajes
  for (i=1; i<3; i++){
    sock = OS_ConnectPort(514, ipconsole);
    if( sock > 0){
      logMessage(0,"Conexion con Socket Exitosa!.");
      SockConn = 1;
      OS_CloseSocket(sock);
      break;
    } else {
      logMessage(0, "Conexion con Socket Fallo!.");
      SockConn = 0;
    }
  }

  maxfd = ifd;
  maxfd++;

  struct numera_data ips = get_interfaces();
    for(controla1 = 0; controla1 < ips.nInterfaces; controla1++)
	{
	  strcat(allIps, prt_interfaces(controla1));
	  strcat(allIps,"|");
	}

  gethostname(hostname, sizeof(hostname));
  hostname[sizeof(hostname) - 1] = '\0';

  while (1) {
	  FD_ZERO(&rfds);
	  FD_SET(ifd, &rfds);
	  ret = select(maxfd, &rfds, NULL, NULL, NULL);
	  if (ret == -1){
		  logMessage(0,"-> Error en Select, terminando aplicacion!! <-");
		  exit(EXIT_FAILURE);
	  }
	  if (!ret)
		  continue;
	  if (FD_ISSET(ifd, &rfds))
		  handle_inotify();
  }

  return 0;
}