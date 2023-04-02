#include "../inc/raycaster.h"
int line_height;
Uint32 buffer[WIN_Y][WIN_X]; // y-coordinate first because it works per scanline
std::vector<int> texture[8];
int init(t_sdl *sdl, t_raycaster *rc)
{
  sdl->window = NULL;
  sdl->renderer = NULL;
  rc->player_pos_x = INIT_P_POS_X;
  rc->player_pos_y = INIT_P_POS_Y;
  rc->player_dir_x = INIT_P_DIR_X;
  rc->player_dir_y = INIT_P_DIR_Y;
  rc->player_plane_x = INIT_P_PLANE_X;
  rc->player_plane_y = INIT_P_PLANE_Y;
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
  {
    fprintf(stderr,"SDL initialization failed (%s)\n",SDL_GetError());
    return (-1);
  }
  if (SDL_CreateWindowAndRenderer(WIN_X, WIN_Y, 0, &sdl->window, &sdl->renderer) != 0)
  {
    fprintf(stderr,"Window creation failed (%s)\n",SDL_GetError());
    return (-1);
  }
  return (0);
}

void generateTexture( )
{
  for(int i = 0; i < 8; i++) texture[i].resize(texWidth * texHeight);

    //generate some textures
  for(int x = 0; x < texWidth; x++)
  for(int y = 0; y < texHeight; y++)
  {
    int xorcolor = (x * 256 / texWidth) ^ (y * 256 / texHeight);
    //int xcolor = x * 256 / texWidth;
    int ycolor = y * 256 / texHeight;
    int xycolor = y * 128 / texHeight + x * 128 / texWidth;
    texture[0][texWidth * y + x] = 65536 * 254 * (x != y && x != texWidth - y); //flat red texture with black cross
    texture[1][texWidth * y + x] = xycolor + 256 * xycolor + 65536 * xycolor; //sloped greyscale
    texture[2][texWidth * y + x] = 256 * xycolor + 65536 * xycolor; //sloped yellow gradient
    texture[3][texWidth * y + x] = xorcolor + 256 * xorcolor + 65536 * xorcolor; //xor greyscale
    texture[4][texWidth * y + x] = 256 * xorcolor; //xor green
    texture[5][texWidth * y + x] = 65536 * 192 * (x % 16 && y % 16); //red bricks
    texture[6][texWidth * y + x] = 65536 * ycolor; //red gradient
    texture[7][texWidth * y + x] = 128 + 256 * 128 + 65536 * 128; //flat grey texture
  }
}

void initial_calc(t_raycaster *rc, int x)
{
  double      camera_x;

  camera_x = 2 * x / (double)(WIN_X) - 1;
  rc->ray_dir_x = rc->player_dir_x + rc->player_plane_x * camera_x;
  rc->ray_dir_y = rc->player_dir_y + rc->player_plane_y * camera_x;
  rc->map_x = (int)(rc->player_pos_x);
  rc->map_y = (int)(rc->player_pos_y);
  rc->delta_dist_x = sqrt(1 + pow(rc->ray_dir_y, 2) / pow(rc->ray_dir_x, 2));
  rc->delta_dist_y = sqrt(1 + pow(rc->ray_dir_x, 2) / pow(rc->ray_dir_y, 2));
  if (rc->ray_dir_x < 0)
  {
    rc->step_x = -1;
    rc->side_dist_x = (rc->player_pos_x - rc->map_x) * rc->delta_dist_x;
  }
  else
  {
    rc->step_x = 1;
    rc->side_dist_x = (rc->map_x + 1.0 - rc->player_pos_x) * rc->delta_dist_x;
  }
  if (rc->ray_dir_y < 0)
  {
    rc->step_y = -1;
    rc->side_dist_y = (rc->player_pos_y - rc->map_y) * rc->delta_dist_y;
  }
  else
  {
    rc->step_y = 1;
    rc->side_dist_y = (rc->map_y + 1.0 - rc->player_pos_y) * rc->delta_dist_y;
  }
}

void          perform_dda(t_raycaster *rc)
{
  int         hit;

  hit = 0;
  while (hit == 0)
  {
    if (rc->side_dist_x < rc->side_dist_y)
    {
      rc->side_dist_x += rc->delta_dist_x;
      rc->map_x += rc->step_x;
      rc->side = 0;
    }
    else
    {
      rc->side_dist_y += rc->delta_dist_y;
      rc->map_y += rc->step_y;
      rc->side = 1;
    }
    if (worldMap[rc->map_x][rc->map_y] > 0)
      hit = 1;
  }
}

void calc_wall_height(t_raycaster *rc)
{
  if (rc->side == 0)
    rc->perp_wall_dist = (rc->map_x - rc->player_pos_x + (1 - rc->step_x) / 2) / rc->ray_dir_x;
  else
    rc->perp_wall_dist = (rc->map_y - rc->player_pos_y + (1 - rc->step_y) / 2) / rc->ray_dir_y;
  line_height = (int)(WIN_Y / rc->perp_wall_dist);
  rc->draw_start = -line_height / 2 + WIN_Y / 2;
  if (rc->draw_start < 0)
    rc->draw_start = 0;
  rc->draw_end = line_height / 2 + WIN_Y / 2;
  if (rc->draw_end >= WIN_Y)
    rc->draw_end = WIN_Y - 1;
}

void texturing(t_raycaster *rc, int x)
{
      //texturing calculations
      int texNum = worldMap[mapWidth - 1][mapHeight - 1] - 1; //1 subtracted from it so that texture 0 can be used!

      //calculate value of wallX
      double wallX; //where exactly the wall was hit
      if (rc->side == 0) wallX = rc->player_pos_y + rc->perp_wall_dist * rc->ray_dir_y;
      else           wallX = rc->player_pos_x + rc->perp_wall_dist * rc->ray_dir_x;
      wallX -= floor((wallX));

      //x coordinate on the texture
      int texX = int(wallX * double(texWidth));
      if(rc->side == 0 && rc->ray_dir_x > 0) texX = texWidth - texX - 1;
      if(rc->side == 1 && rc->ray_dir_y < 0) texX = texWidth - texX - 1;

                  // How much to increase the texture coordinate per screen pixel
      double step = 1.0 * texHeight / line_height;
      // Starting texture coordinate
      double texPos = (rc->draw_start - WIN_Y / 2 + line_height / 2) * step;
      for(int y = rc->draw_start; y< rc->draw_end; y++)
      {
        // Cast the texture coordinate to integer, and mask with (texHeight - 1) in case of overflow
        int texY = (int)texPos & (texHeight - 1);
        texPos += step;
        Uint32 color = texture[texNum][texHeight * texY + texX];
        //make color darker for y-sides: R, G and B byte each divided through two with a "shift" and an "and"
        if(rc->side == 1) color = (color >> 1) & 8355711;
        buffer[y][x] = color;
      }
}

void   draw_vert_line(t_sdl *sdl, t_raycaster *rc, int x)
{
  SDL_Color   color;
  
  color = apply_night_effect(select_wall_color(rc->map_x, rc->map_y), rc->perp_wall_dist);

  if (rc->side == 1)
  {
    color.r /= 2;
    color.g /= 2;
    color.b /= 2;
  }
  SDL_SetRenderDrawColor(sdl->renderer, color.r, color.g, color.b, SDL_ALPHA_OPAQUE);
  SDL_RenderDrawLine(sdl->renderer, x, rc->draw_start, x, rc->draw_end);

}

void          render_frame(t_sdl *sdl)
{
  SDL_RenderPresent(sdl->renderer);
  SDL_SetRenderDrawColor(sdl->renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(sdl->renderer);
}

int           handle_events(t_raycaster *rc)
{
  SDL_Event   event;
  double      oldDirX;
  double      oldPlaneX;
  const Uint8 *keyboardState = SDL_GetKeyboardState( NULL );
  while (SDL_PollEvent(&event))
  {
    if (event.type == SDL_QUIT)
      return (-1);
    if (event.type == SDL_KEYDOWN)
    {
      if (event.key.keysym.sym == SDLK_UP || keyboardState[ SDL_SCANCODE_W ] )
      {
        if(worldMap[(int)(rc->player_pos_x + rc->player_dir_x * MV_SPEED)][(int)(rc->player_pos_y)] == 0) rc->player_pos_x += rc->player_dir_x * MV_SPEED;
        if(worldMap[(int)(rc->player_pos_x)][(int)(rc->player_pos_y + rc->player_dir_y * MV_SPEED)] == 0) rc->player_pos_y += rc->player_dir_y * MV_SPEED;
      }
      if (event.key.keysym.sym == SDLK_DOWN || keyboardState[ SDL_SCANCODE_S ] )
      {
        if(worldMap[(int)(rc->player_pos_x - rc->player_dir_x * MV_SPEED)][(int)(rc->player_pos_y)] == 0) rc->player_pos_x -= rc->player_dir_x * MV_SPEED;
        if(worldMap[(int)(rc->player_pos_x)][(int)(rc->player_pos_y - rc->player_dir_y * MV_SPEED)] == 0) rc->player_pos_y -= rc->player_dir_y * MV_SPEED;
      }
      if (event.key.keysym.sym == SDLK_RIGHT || keyboardState[ SDL_SCANCODE_D ] )
      {
        oldDirX = rc->player_dir_x;
        rc->player_dir_x = rc->player_dir_x * cos(-ROT_SPEED) - rc->player_dir_y * sin(-ROT_SPEED);
        rc->player_dir_y = oldDirX * sin(-ROT_SPEED) + rc->player_dir_y * cos(-ROT_SPEED);
        oldPlaneX = rc->player_plane_x;
        rc->player_plane_x = rc->player_plane_x * cos(-ROT_SPEED) - rc->player_plane_y * sin(-ROT_SPEED);
        rc->player_plane_y = oldPlaneX * sin(-ROT_SPEED) + rc->player_plane_y * cos(-ROT_SPEED);
      }
      if (event.key.keysym.sym == SDLK_LEFT || keyboardState[ SDL_SCANCODE_A ] )
      {
        oldDirX = rc->player_dir_x;
        rc->player_dir_x = rc->player_dir_x * cos(ROT_SPEED) - rc->player_dir_y * sin(ROT_SPEED);
        rc->player_dir_y = oldDirX * sin(ROT_SPEED) + rc->player_dir_y * cos(ROT_SPEED);
        oldPlaneX = rc->player_plane_x;
        rc->player_plane_x = rc->player_plane_x * cos(ROT_SPEED) - rc->player_plane_y * sin(ROT_SPEED);
        rc->player_plane_y = oldPlaneX * sin(ROT_SPEED) + rc->player_plane_y * cos(ROT_SPEED);
      }
    }
  }
  return (0);
}

void raycaster(t_sdl *sdl, t_raycaster *rc)
{
  SDL_bool done;

  done = SDL_FALSE;
  while(!done)
  {
    for(int x = 0; x < WIN_X; x++)
    {
      initial_calc(rc, x);
      perform_dda(rc);
      calc_wall_height(rc);
      texturing(rc, x);
      // draw_vert_line(sdl, rc, x);
    }
    drawbuffer(sdl, buffer[0]);
    for(int y = 0; y < WIN_Y; y++) for(int x = 0; x < WIN_X; x++) buffer[y][x] = 0; //clear the buffer instead of cls()
    render_frame(sdl);
    if (handle_events(rc) != 0)
      done = SDL_TRUE;
  }
}

int main()
{
  t_sdl       sdl;
  t_raycaster rc;

  if (init(&sdl, &rc) != 0)
    return (-1);
  generateTexture();
  raycaster(&sdl, &rc);
  if (sdl.renderer)
    SDL_DestroyRenderer(sdl.renderer);
  if (sdl.window)
    SDL_DestroyWindow(sdl.window);
  SDL_Quit();
  return (0);
}