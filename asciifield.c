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
    double *db;

    double m[16];
    double n, f, aspect, theta;

    double speed;
    double fps;
    int num_stars;
    int first;

    int draw_ship;
    double ship_wobble_x, ship_wobble_y;
    double ship_off_x, ship_off_y;
};

struct star
{
    double v[4];
    struct star *next;
};

void
init(struct screen *s)
{
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
    s->db = malloc(sizeof(double) * s->width * s->height);
    if (s->db == NULL)
    {
        fprintf(stderr, "malloc for db failed\n");
        exit(EXIT_FAILURE);
    }

    /* Clipping planes, font aspect ratio, FOV 45 degree. */
    s->n = -0.1;
    s->f = -10;
    s->aspect = 0.5;
    s->theta = 45 * DEG_2_RAD;

    /* Ship parameters. Wobble speed is an angular velocity. */
    s->draw_ship = 0;
    s->ship_wobble_x = 0.125 * 360 * DEG_2_RAD;
    s->ship_wobble_y = -0.165 * 360 * DEG_2_RAD;
    s->ship_off_x = 0;
    s->ship_off_y = 0;

    /* Misc options. Speed is "units per second". */
    s->speed = 4;
    s->fps = 30;
    s->num_stars = 300;
    s->first = 1;
}

void
init_m(struct screen *s)
{
    int i;

    /* Initialize projection matrix according to our PDF. */
    i = 0;
    s->m[i++] = 1.0 / tan(s->theta * 0.5) / s->aspect;
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
    s->m[i++] = 1;

    s->m[i++] = 0;
    s->m[i++] = 0;
    s->m[i++] = (-2 * s->n * s->f) / (s->f - s->n);
    s->m[i++] = 0;
}

void
clear(struct screen *s)
{
    int i;

    memset(s->fb, ' ', s->width * s->height);
    for (i = 0; i < s->width * s->height; i++)
        s->db[i] = 1;
}

void
draw(struct screen *s, double *v_orig, double *v)
{
    double x_p, y_p, len2, depth;
    char c;

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
    {
        /* Projected z values range from -1 (closest to n and screen)
         * to 1 (closest to f). */
        depth = s->db[(int)(y_p) * s->width + (int)(x_p)];
        if (v[2] < depth)
        {
            s->fb[(int)(y_p) * s->width + (int)(x_p)] = c;
            s->db[(int)(y_p) * s->width + (int)(x_p)] = v[2];
        }
    }
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
        if (y < s->height - 1)
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
}

void
random_star(struct screen *s, struct star *st, int initial)
{
    st->v[0] = (drand48() * 2 - 1) * 4 * s->aspect;
    st->v[1] = (drand48() * 2 - 1) * 4;
    st->v[2] = initial ? -(drand48() * 9 + 1) : s->f;
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
        if (p->v[2] > s->n || p->v[2] < s->f)
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
    int existing;

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
time_diff(struct timeval *t1, struct timeval *t2)
{
    double t1_s, t2_s;

    t1_s = (double)t1->tv_sec + (double)t1->tv_usec / 1e6;
    t2_s = (double)t2->tv_sec + (double)t2->tv_usec / 1e6;

    return t2_s - t1_s;
}


double
calc_stepsize(struct screen *s, struct timeval *t1, struct timeval *t2)
{
    return s->speed * time_diff(t1, t2);
}

void
ship(struct screen *s)
{
    /* That's a little enterprise. Bytes which are '#' will be
     * transparent. */
    const int ship_w = 21, ship_h = 7;
    char *shipstr[] = {"##_######_-_######_##",
                       "_|_|.---'---`---.|_|_",
                       "\\-`.-.___O_O___.-.'-/",
                       "####`.##`]-['##,'####",
                       "######`.' _ `.'######",
                       "#######| /_\\ |#######",
                       "########`___'########"};
    char pic;
    int x0, y0, x, y, x_p, y_p;

    x0 = (int)((s->width - ship_w) * 0.5);
    y0 = (int)((s->height - ship_h) * 0.5);

    for (y = 0; y < ship_h; y++)
    {
        for (x = 0; x < ship_w; x++)
        {
            pic = shipstr[y][x];
            if (pic != '#')
            {
                x_p = x0 + x + (int)s->ship_off_x;
                y_p = y0 + y + (int)s->ship_off_y;
                if (x_p >= 0 && x_p < s->width && y_p >= 0 && y_p < s->height)
                    s->fb[y_p * s->width + x_p] = pic;
            }
        }
    }
}

void
update_ship_offset(struct screen *s, struct timeval *t1, struct timeval *t2)
{
    s->ship_off_x = sin(s->ship_wobble_x * time_diff(t1, t2)) * 4;
    s->ship_off_y = sin(s->ship_wobble_y * time_diff(t1, t2)) * 2;
}

int
main(int argc, char **argv)
{
    double v_p[4];
    double stepsize;
    struct screen s;
    struct star *field = NULL, *p;
    struct timeval t0, t1, t2;
    int opt;

    init(&s);

    while ((opt = getopt(argc, argv, "es:n:f:")) != -1)
    {
        switch (opt)
        {
            case 'e':
                s.draw_ship = 1;
                break;
            case 's':
                s.speed = atof(optarg);
                break;
            case 'n':
                s.num_stars = atoi(optarg);
                break;
            case 'f':
                s.aspect = atof(optarg);
                break;
        }
    }

    init_m(&s);


    /* Hide cursor and restore it when we're exiting. */
    printf("\033[?25l");
	signal(SIGINT, cleanup_terminal);
	signal(SIGHUP, cleanup_terminal);
	signal(SIGTERM, cleanup_terminal);

    gettimeofday(&t1, NULL);
    t0 = t1;

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

        if (s.draw_ship)
            ship(&s);

        show(&s);

        /* Depending on how much time has passed, calculate the required
         * step size to get the configured speed. */
        gettimeofday(&t2, NULL);
        stepsize = calc_stepsize(&s, &t1, &t2);
        update_ship_offset(&s, &t0, &t2);
        t1 = t2;

        for (p = field; p != NULL; p = p->next)
            p->v[2] += stepsize;

        /* This is just an upper limit. If we're doing less fps, the
         * speed will remain the same. */
        usleep((1.0 / s.fps) * 1e6);
    }
}
