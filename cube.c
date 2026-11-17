#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
  int w,h,scale_x,scale_y;
} view;

typedef struct {
  float x,y,z;
} vec3;

typedef struct {
  float x,y;
} vec2;

typedef struct {
  vec3 *verts;
  int (*edge_table)[2];
  int vert_count;
  int edge_count;
} obj;

typedef struct {
  float x,y,depth;
} vert;

typedef struct {
  vert *verts;
  int (*edge_table)[2];
  int vert_count;
  int edge_count;
} graph;

void spawn_cube(obj *cube) {
  cube->vert_count = 8;
  cube->verts = (vec3*)malloc(8*sizeof(vec3));
  
  cube->verts[0] = (vec3){-1.0f, -1.0f, -1.0f};
  cube->verts[1] = (vec3){1.0f, -1.0f, -1.0f};
  cube->verts[2] = (vec3){1.0f, -1.0f, 1.0f};
  cube->verts[3] = (vec3){-1.0f, -1.0f, 1.0f};
  cube->verts[4] = (vec3){-1.0f, 1.0f, -1.0f};
  cube->verts[5] = (vec3){1.0f, 1.0f, -1.0f};
  cube->verts[6] = (vec3){1.0f, 1.0f, 1.0f};
  cube->verts[7] = (vec3){-1.0f, 1.0f, 1.0f};

  int cube_edges[12][2] = {
    // 底面 4 条边
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    // 顶面 4 条边
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    // 垂直 4 条边
    {0, 4}, {1, 5}, {2, 6}, {3, 7}
  };
  cube->edge_table = malloc(sizeof(cube_edges));
  memcpy(cube->edge_table, cube_edges, sizeof(cube_edges));
  cube->edge_count = 12;
}

void spin_rad(obj *o, vec3 axis) {
  
  float cx = cosf(axis.x), sx = sinf(axis.x);
  float cy = cosf(axis.y), sy = sinf(axis.y);
  float cz = cosf(axis.z), sz = sinf(axis.z);

  for (int i = 0; i < o->vert_count; i++) {
        float x = o->verts[i].x;
        float y = o->verts[i].y;
        float z = o->verts[i].z;

        float y1 = y * cx - z * sx;
        float z1 = y * sx + z * cx;
        float x1 = x;

        float x2 = x1 * cy + z1 * sy;
        float z2 = -x1 * sy + z1 * cy;
        float y2 = y1;

        float x3 = x2 * cz - y2 * sz;
        float y3 = x2 * sz + y2 * cz;
        float z3 = z2;
        o->verts[i].x = x3;
        o->verts[i].y = y3;
        o->verts[i].z = z3;
  }
}

void render(obj *o, graph *out, float z_offset) {
  for (int i = 0; i < o->vert_count; i++) {
    out->verts[i].x = o->verts[i].x;
    out->verts[i].y = o->verts[i].y;
    out->verts[i].depth = o->verts[i].z + z_offset;
    // point will not be drawed if depth below 0.
  }
}

void draw(graph *g, view v, char *buf, float *zbuf) {
  memset(zbuf, 0, (v.h * v.w) * sizeof(float));
  memset(buf, ' ', (v.w + 1) * v.h);

  for (int i = 0; i < v.h; i++) {
    int idx = (1 + i) * (v.w + 1) - 1;
    buf[idx] = '\n';
  }
  
  for (int i = 0; i < g->vert_count; i++) {
    int c = (v.w / 2) + (int)(g->verts[i].x * v.scale_x);
    int r = (v.h / 2) - (int)(g->verts[i].y * v.scale_y);

    if (c < 1 || c > v.w || r < 1 || r > v.h) {
      continue;
    }

    int idx_zbuf = v.w * (r - 1) + c - 1;
    if (g->verts[i].depth < zbuf[idx_zbuf]) {
      continue;
    }

    int idx_buf = (v.w + 1) * (r - 1) + c - 1;
    buf[idx_buf] = '@'; 
    zbuf[idx_zbuf] = g->verts[i].depth;
  }
  
  for (int i = 0; i < g->edge_count; i++) {
    vert start = g->verts[g->edge_table[i][0]];
    vert end = g->verts[g->edge_table[i][1]];

    int x1 = (v.w / 2) + (int)(start.x * v.scale_x),
    y1 = (v.h / 2) - (int)(start.y * v.scale_y);
    
    int x2 = (v.w / 2) + (int)(end.x * v.scale_x),
    y2 = (v.h / 2) - (int)(end.y * v.scale_y);

    int steep = abs(y2 - y1) > abs(x2 - x1);
    if (steep) {
      int temp = x1; x1 = y1; y1 = temp;
      temp = x2; x2 = y2; y2 = temp;
    }
    
    int dx = abs(x1 - x2);
    int dy = abs(y1 - y2);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int p = 2 * dy - dx;
    
    float dd = (dx == 0) ? 0.0f : (end.depth - start.depth) / dx;

    for (int i = 0; i <= dx; i++) {
      if (x1 == x2 && y1 == y2) {
        break;
      }
      if (p >= 0) {
        x1 += sx;
        y1 += sy;
        p += 2 * (dy - dx);
      } else {
        x1 += sx;
        p += 2 * dy;
      }

      int x,y;
      if (steep) {
        x = y1;
        y = x1;
      } else {
        x = x1;
        y = y1;
      }
      float depth = start.depth + dd * (i + 1);
  
      if (x < 1 || x > v.w || y < 1 || y > v.h) {
        continue;
      }

      int idx_zbuf = v.w * (y - 1) + x - 1;
      if (depth < zbuf[idx_zbuf]) {
        continue;
      }

      int idx_buf = (v.w + 1) * (y - 1) + x - 1;
      if (buf[idx_buf] == '@') {
        continue;
      }
      buf[idx_buf] = '*'; 
      zbuf[idx_zbuf] = depth;
    
    }
    
  }

}

volatile sig_atomic_t keep_running = 1;
void handle_sigint(int sig) {
  (void)sig;
  keep_running = 0;
}

int main() {
  signal(SIGINT, handle_sigint);

  const char *reset = "\033[H";
  const char *enter = "\033[?1049h";
  const char *exit = "\033[?1049l";

  obj cube;
  spawn_cube(&cube);

  view screen = {
    .w = 32,
    .h = 32,
    .scale_x = 10,
    .scale_y = 5,
  };

  graph gph = {
    .verts = malloc(cube.vert_count * sizeof(vert)),
    .vert_count = cube.vert_count,
    .edge_count = cube.edge_count,
    .edge_table = cube.edge_table,
  };
  char *buf = malloc((screen.w + 1) * screen.h);
  float *zbuf = malloc((screen.h * screen.w) * sizeof(float));

  vec3 speed = {0.01f, 0.02f, 0.01f};

  write(STDOUT_FILENO, enter, 8);
  while (keep_running) {
    spin_rad(&cube, speed);
    render(&cube, &gph, 100.0f);
    draw(&gph, screen, buf, zbuf);
    write(STDOUT_FILENO, reset, 3);
    write(STDOUT_FILENO, buf, (screen.w + 1) * screen.h);
    usleep(30000);
  }
  write(STDOUT_FILENO, exit, 8);
  
  free(buf);
  free(zbuf);
  free(cube.verts);
  free(cube.edge_table);

  return EXIT_SUCCESS;
}
