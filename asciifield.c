#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define DEG_2_RAD (M_PI / 180.0)

struct screen
{
    int width, height;
    char *fb;

    double m[16];
    double n, f, aspect, theta;

    double speed;
    double fps;
    size_t num_stars;
    int first;
};

struct star
{
    double v[4];
    struct star *next;
};

void
init(struct screen *s)
{
    int i;
    struct winsize w;

    if (isatty(STDOUT_FILENO))
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    else
    {
        w.ws_col = 80;
        w.ws_row = 24;
    }

    s->width = w.ws_col;
    s->height = w.ws_row;
    s->fb = malloc(s->width * s->height);
    if (s->fb == NULL)
    {
        fprintf(stderr, "malloc for fb failed\n");
        exit(EXIT_FAILURE);
    }

    /* Clipping planes, unused aspect ratio, FOV 45 degree. */
    s->n = 1;
    s->f = 10;
    s->aspect = 1;
    s->theta = 45 * DEG_2_RAD;

    /* Initialize standard OpenGL-ish projection matrix. */
    i = 0;
    s->m[i++] = 1.0 / tan(s->theta * 0.5 * s->aspect);
    s->m[i++] = 0;
    s->m[i++] = 0;
    s->m[i++] = 0;

    s->m[i++] = 0;
    s->m[i++] = 1.0 / tan(s->theta * 0.5);
    s->m[i++] = 0;
    s->m[i++] = 0;

    s->m[i++] = 0;
    s->m[i++] = 0;
    s->m[i++] = (s->f + s->n) / (s->f - s->n);
    s->m[i++] = -1;

    s->m[i++] = 0;
    s->m[i++] = 0;
    s->m[i++] = -(2 * s->n * s->f) / (s->f - s->n);
    s->m[i++] = 0;

    /* Misc options. */
    s->speed = 3;
    s->fps = 30;
    s->num_stars = 300;
    s->first = 1;
}

void
clear(struct screen *s)
{
    memset(s->fb, ' ', s->width * s->height);
}

void
draw(struct screen *s, double *v_orig, double *v)
{
    double x_p, y_p, len2;
    char c;

    /* Clipping. */
    if (v[2] >= 0)
        return;

    /* Set "character size" depending on distance to camera. */
    len2 = v_orig[0] * v_orig[0] + v_orig[1] * v_orig[1] + v_orig[2] * v_orig[2];
    if (len2 > 50)
        c = '.';
    else if (len2 > 20)
        c = '*';
    else
        c = '@';

    /* Scale to screen pixels. */
    x_p = (v[0] + 1) * 0.5 * s->width;
    y_p = (v[1] + 1) * 0.5 * s->height;

    if (x_p >= 0 && x_p < s->width && y_p >= 0 && y_p < s->height)
        s->fb[(int)(y_p) * s->width + (int)(x_p)] = c;
}

void
show(struct screen *s)
{
    int x, y;

    for (y = 0; y < s->height; y++)
    {
        for (x = 0; x < s->width; x++)
            printf("%c", s->fb[y * s->width + x]);

        /* Do not print a newline in the very last line to avoid
         * flickering. */
        if (y + 1 < s->height - 1)
            printf("\n");
    }
}

void
project(struct screen *s, double *v, double *v_p)
{
    /* Project vector. */
    v_p[0] = v[0] * s->m[0] + v[1] * s->m[4] + v[2] * s->m[8] + v[3] * s->m[12];
    v_p[1] = v[0] * s->m[1] + v[1] * s->m[5] + v[2] * s->m[9] + v[3] * s->m[13];
    v_p[2] = v[0] * s->m[2] + v[1] * s->m[6] + v[2] * s->m[10] + v[3] * s->m[14];
    v_p[3] = v[0] * s->m[3] + v[1] * s->m[7] + v[2] * s->m[11] + v[3] * s->m[15];

    /* "Dehomogenize". */
    v_p[0] /= v_p[3];
    v_p[1] /= v_p[3];
    v_p[2] /= v_p[3];

    /* Divide by z. */
    v_p[0] /= v_p[2];
    v_p[1] /= v_p[2];
}

void
random_star(struct screen *s, struct star *st, int initial)
{
    st->v[0] = (drand48() * 2 - 1) * 4;
    st->v[1] = (drand48() * 2 - 1) * 4;
    st->v[2] = initial ? drand48() * 9 + 1 : s->f;
    st->v[3] = 1;
    st->next = NULL;
}

void
cleanup_stars(struct screen *s, struct star **field)
{
    struct star *p, *prev = NULL;

    /* If a star is outside the frustum, then remove it and make room
     * for a new one. */

    for (p = *field; p != NULL; p = p->next)
    {
        if (p->v[2] < s->n || p->v[2] > s->f)
        {
            if (p == *field)
            {
                /* Delete very first star, hence update the "public"
                 * pointer. */
                *field = (*field)->next;
                free(p);

                p = *field;
            }
            else
            {
                /* Deletion of any other star. We must first find the
                 * last list element. */
                for (prev = *field; prev->next != p; prev = prev->next)
                    /* noop */ ;

                prev->next = p->next;
                free(p);

                p = prev;
            }
        }
    }
}

void
ensure_stars(struct screen *s, struct star **field)
{
    struct star *p, *last = NULL;
    size_t existing;

    if (*field == NULL)
    {
        /* Create an initial list element if there is none yet. */
        srand48(time(0));
        *field = malloc(sizeof(struct star));
        random_star(s, *field, s->first);
    }

    /* Count existing stars. */
    existing = 0;
    for (p = *field; p != NULL; p = p->next)
    {
        last = p;
        existing++;
    }

    /* Create missing stars. */
    while (existing < s->num_stars)
    {
        p = malloc(sizeof(struct star));
        random_star(s, p, s->first);
        last->next = p;
        last = p;
        existing++;
    }

    /* Only the very first pass will create stars all over the place.
     * All subsequent passes will create stars at z = far. */
    s->first = 0;
}

void
cleanup_terminal(int dummy)
{
	(void)dummy;

	printf("\e[?12l\e[?25h");
	exit(EXIT_SUCCESS);
}

double
calc_stepsize(struct screen *s, struct timeval *t1, struct timeval *t2)
{
    double t1_s, t2_s, diff;

    t1_s = (double)t1->tv_sec + (double)t1->tv_usec / 1e6;
    t2_s = (double)t2->tv_sec + (double)t2->tv_usec / 1e6;

    diff = t2_s - t1_s;

    return s->speed * diff;
}

int
main()
{
    double v_p[4];
    double stepsize;
    struct screen s;
    struct star *field = NULL, *p;
    struct timeval t1, t2;

    init(&s);

    /* Hide cursor and restore it when we're exiting. */
    printf("\033[?25l");
	signal(SIGINT, cleanup_terminal);
	signal(SIGHUP, cleanup_terminal);
	signal(SIGTERM, cleanup_terminal);

    gettimeofday(&t1, NULL);

    while (1)
    {
        /* Jump back to top left corner. Do not clear anything. Each
         * pixel will be overwritten one at a time. This avoids
         * flickering. */
        printf("\033[H");

        cleanup_stars(&s, &field);
        ensure_stars(&s, &field);
        clear(&s);

        for (p = field; p != NULL; p = p->next)
        {
            project(&s, p->v, v_p);
            draw(&s, p->v, v_p);
        }

        show(&s);

        /* Depending on how much time has passed, calculate the required
         * step size to get the configured speed. */
        gettimeofday(&t2, NULL);
        stepsize = calc_stepsize(&s, &t1, &t2);
        t1 = t2;

        for (p = field; p != NULL; p = p->next)
            p->v[2] -= stepsize;

        /* This is just an upper limit. If we're doing less fps, the
         * speed will remain the same. */
        usleep((1.0 / s.fps) * 1e6);
    }
}
