#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "build_info.h"
#include "logs.h"
#include "envvars.h"
#include "../../version.h"

void print_build_infos()
{
	SHUT_LOGD("v%d.%d.%d built on %s %s\n", MAJOR, MINOR, REVISION, __DATE__, __TIME__);
}

// Banner de Lorica: una vez por proceso, solo si stderr es una terminal.
// LIBGL_BANNER=0 lo apaga; NO_COLOR (o TERM=dumb) lo deja sin colores.
static const char *logo[] = {
	" _     ___  ____  ___ ____    _    ",
	"| |   / _ \\|  _ \\|_ _/ ___|  / \\   ",
	"| |  | | | | |_) || | |     / _ \\  ",
	"| |__| |_| |  _ < | | |___ / ___ \\ ",
	"|_____\\___/|_| \\_\\___\\____/_/   \\_\\",
};
static const int grad[] = { 51, 45, 39, 33, 27 };

void lorica_banner(void)
{
	if(IsEnvVarFalse("LIBGL_BANNER")) return;
	if(!isatty(fileno(stderr))) return;
	const char *nc = getenv("NO_COLOR"), *term = getenv("TERM");
	int color = !(nc && *nc) && !(term && !strcmp(term, "dumb"));
	int i, n = (int)(sizeof(logo)/sizeof(logo[0]));
	fputc('\n', stderr);
	for(i=0; i<n; i++) {
		if(color) fprintf(stderr, "  \x1b[1;38;5;%dm%s\x1b[0m\n", grad[i], logo[i]);
		else fprintf(stderr, "  %s\n", logo[i]);
	}
	if(color) fprintf(stderr, "\n  \x1b[1;38;5;51mOpenGL sobre el GLES nativo de Android\x1b[0m \x1b[38;5;245m- presentacion cero-copia\x1b[0m\n");
	else fprintf(stderr, "\n  OpenGL sobre el GLES nativo de Android - presentacion cero-copia\n");
	if(color) fprintf(stderr, "  \x1b[38;5;245mbasado en gl4es de ptitSeb (MIT) | LIBGL_BANNER=0 oculta esto\x1b[0m\n\n");
	else fprintf(stderr, "  basado en gl4es de ptitSeb (MIT) | LIBGL_BANNER=0 oculta esto\n\n");
	fflush(stderr);
}
