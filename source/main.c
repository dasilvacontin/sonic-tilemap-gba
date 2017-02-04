
#include <stdio.h>
#include <stdlib.h>
#include <tonc.h>
#include "all_gfx.h"

SCR_ENTRY map[32][32];

#define BLOCK 0
#define BLOCK_EDGE 1
#define BLOCK_ALONE 2
#define BLOCK_INNER 3
#define BLOCK_UNDER 4

#define COIN0 5
#define COIN1 6
#define COIN2 7

#define SONIC 8
#define SSPIN0 9
#define SSPIN1 10
#define BADNIK 14

#define TRAIL 11
#define EXPLOSION0 15
#define EXPLOSION1 16
#define EXPLOSION2 17
#define EMPTY 18

#define die_if_badnik(tx, ty, sonic) if (!is_attacking(sonic) && get_tile_sprite(tx, ty) == BADNIK) { ty = 100; continue; }

inline int is_attacking (int sonic) {
  int sprite = sonic & SE_ID_MASK;
  return (sprite == SSPIN0 || sprite == SSPIN1);
}

inline int set_sprite(int value, int sprite) {
  return (value & ~SE_ID_MASK) | sprite;
}

inline int get_tile_value(int x, int y) {
  return se_mem[30][x + y * 32];
}

inline int get_tile_id(int x, int y) {
  return get_tile_value(x, y) & SE_ID_MASK;
}

inline int get_tile_sprite(int x, int y) {
  return get_tile_value(x, y) & SE_ID_MASK;
}

inline void set_tile(int x, int y, int value) {
  se_mem[30][x + y * 32] = value;
}

int is_sprite_block (int sprite) {
  return (
    sprite == BLOCK ||
    sprite == BLOCK_EDGE ||
    sprite == BLOCK_UNDER ||
    sprite == BLOCK_INNER ||
    sprite == BLOCK_ALONE
  );
}
int is_block (int x, int y, int sprite) {
  if (x < 0 || y < 0 || x >= 32 || y >= 32) return 1;
  int tile_id = get_tile_id(x, y);
  return (
    is_sprite_block(tile_id) ||
    (tile_id == BADNIK && sprite == BADNIK)
  );
}

int is_walkable(int x, int y) {
  return !is_block(x, y, 0);
}

int can_walk(int x, int y, int sprite) {
  return !is_block(x, y, sprite);
}

int main()
{
	// Init interrupts and VBlank irq.
	irq_init(NULL);
	irq_add(II_VBLANK, NULL);

  /* map generation */
  int tile_value;
  int last_is_wall = 0;
  for (int i = 0; i < 32; ++i) {
    for (int j = 0; j < 32; ++j) {
      tile_value = (rand() % 7);
      if (tile_value < (3 - last_is_wall)) tile_value = EMPTY;
      if (tile_value == 6) tile_value = (rand() % 4 == 0) ? BADNIK : EMPTY;
      map[i][j] = tile_value;
      last_is_wall = is_sprite_block(tile_value);
    }
  }

	// Video mode 0, enable bg 0.
  // bitmap BG2 MODE(3),4,5 (3: colors 16bits (5.5.5), 4: colors 8bits con pageflipping, 5: pageflipping)
  // tilemade BG0123 MODE(0),1,2 (0: 4 layers/bgs, 1: 2 normales y 1 affines, 2: 2 afines)
	REG_DISPCNT= DCNT_MODE0 | DCNT_BG0;

  memcpy(&tile8_mem[0][0], blocksTiles, blocksTilesLen);
  memcpy(pal_bg_mem, blocksPal, blocksPalLen);
  memcpy(&se_mem[30][0], map, sizeof(map));

  REG_BG0CNT = BG_CBB(0) // charblock 0
    | BG_SBB(30) // screenblock 30 (donde se guarda el map)
    | BG_8BPP // (las tiles utilizan) 8 bits por pixel
    | BG_REG_32x32; // el mapa tiene 32 x 32 tiles

  int x = 14;
  int y = 7;
  se_mem[30][x + y * 32] = SONIC;

  /* map normalization */
  for (int i = 0; i < 32; ++i) {
    for (int j = 0; j < 32; ++j) {
      int is_wall = !is_walkable(j, i);
      if (!is_wall) continue;

      int wup = !is_walkable(j, i - 1);
      int wdown = !is_walkable(j, i + 1);
      int wleft = !is_walkable(j - 1, i);
      int wright = !is_walkable(j + 1, i);

      int value;
      if (wup) {
        if (wdown || (wleft && wright) || (!wleft && !wright)) value = BLOCK_INNER;
        else if (wleft) value = BLOCK_UNDER;
        else value = BLOCK_UNDER | SE_HFLIP;
      } else if (wleft && !wright) {
        value = BLOCK_EDGE;
      } else if (!wleft && wright) {
        value = BLOCK_EDGE | SE_HFLIP;
      } else if (wleft && wright) {
        value = BLOCK;
      } else {
        value = BLOCK_ALONE;
      }
      set_tile(j, i, value);
    }
  }

  int dt = 0;
  int enemy_logic = 0;
	while(1)
	{
		VBlankIntrWait();
    dt++;
    if (dt != 7) continue;

    dt = 0;
    ++enemy_logic;
    int enemy_active = (enemy_logic == 10);
    if (enemy_logic == 10) enemy_logic = 0;
    key_poll();

    int tile_value;
    for (int i = 32; i >= 0; i--) {
      for (int j = 0; j < 32; ++j) {
        tile_value = get_tile_value(j, i);
        int sprite = tile_value & SE_ID_MASK;

        // coin animation
        if (COIN0 <= sprite && sprite <= COIN2) {
          int coin_dir = tile_value & SE_HFLIP;
          if (coin_dir) sprite--;
          else sprite++;

          if (sprite == COIN0) {
            tile_value = COIN0;
          } else if (sprite == COIN2) {
            tile_value = COIN2 | SE_HFLIP;
          } else {
            tile_value = sprite | coin_dir;
          }
          set_tile(j, i, tile_value);
        }

        // SONIC spin animation
        if (sprite == SSPIN0 || sprite == SSPIN1) {
          sprite++;
          if (sprite > SSPIN1) sprite = SSPIN0;
          set_tile(j, i, sprite | (tile_value & ~SE_ID_MASK));
        }

        // Sonic TRAIL animation
        if (sprite == TRAIL) {
          if (tile_value & SE_VFLIP) set_tile(j, i, EMPTY);
          else set_tile(j, i, TRAIL | SE_VFLIP);
        }

        // BADNIK LOGIC
        if (sprite == BADNIK) {
          int x = j;
          int y = i;
          if (can_walk(x, y + 1, BADNIK)) set_tile(x, y++, EMPTY);
          else if (enemy_active) {
            int going_left = tile_value & SE_HFLIP;
            int xinc = going_left ? -1 : 1;
            if (can_walk(x + xinc, y, BADNIK) && !can_walk(x + xinc, y + 1, 0)) {
              set_tile(x, y, EMPTY);
              x += xinc;
              if (xinc == 1) ++j; // avoid re-evaluation
            } else { // switch dir
              if (going_left) tile_value = tile_value & ~SE_HFLIP;
              else tile_value = tile_value | SE_HFLIP;
            }
          }
          set_tile(x, y, tile_value);
        }

        // EXPLOSION animation
        if (sprite >= EXPLOSION0 && sprite <= EXPLOSION2) {
          sprite++;
          set_tile(j, i, sprite);
        }
      }
    }

    int sonic_obj = get_tile_value(x, y);
    int walkable_left = is_walkable(x - 1, y);
    int walkable_right = is_walkable(x + 1, y);
    int walkable_up = is_walkable(x, y - 1);
    int walkable_down = is_walkable(x, y + 1);
    int is_looking_left = sonic_obj & SE_HFLIP;

    if (walkable_down || is_attacking(sonic_obj)) {
      // air horizontal control
      if (key_is_down(KEY_LEFT)) {
        if (walkable_left && is_looking_left) set_tile(x--, y, TRAIL);
        sonic_obj |= SE_HFLIP;
      } else if (key_is_down(KEY_RIGHT)) {
        if (walkable_right && !is_looking_left) set_tile(x++, y, TRAIL);
        sonic_obj &= ~SE_HFLIP;
      }

      // air vertical component, either upwards jump, or falling
      walkable_down = is_walkable(x, y + 1);
      if (walkable_down) { // in case it didnt land after horizontal air control
        set_tile(x, y, TRAIL);
        int going_up = sonic_obj & SE_VFLIP;
        if (going_up) { // jump in process
          walkable_up &= is_walkable(x, y - 1);
          if (walkable_up) y--;
          sonic_obj &= ~SE_VFLIP; // cenit, start falling down
        } else y++; // falling
      }
    } else {
      // on the ground
      int key_jump = key_is_down(KEY_UP) | key_is_down(KEY_A);
      if (key_jump && walkable_up) {
        // jump
        set_tile(x, y--, TRAIL);
        sonic_obj = set_sprite(sonic_obj, SSPIN0) | SE_VFLIP; // vflip denotes upwards velocity
      }


      // horizontal air control after jump
      // can only move if didn't have any horizontal obstacles at any moment
      walkable_left &= is_walkable(x - 1, y);
      walkable_right &= is_walkable(x + 1, y);
      if (key_is_down(KEY_LEFT)) {
        if (walkable_left && is_looking_left) set_tile(x--, y, TRAIL);
        sonic_obj |= SE_HFLIP;
      } else if (key_is_down(KEY_RIGHT) ) {
        if (walkable_right && !is_looking_left) set_tile(x++, y, TRAIL);
        sonic_obj &= ~SE_HFLIP;
      }
    }

    // collision with badnik
    if (get_tile_sprite(x, y) == BADNIK) {
      if (is_attacking(sonic_obj)) {
        sonic_obj = set_sprite(sonic_obj, SSPIN0) | SE_VFLIP;
        set_tile(x, y, EXPLOSION0);
        --y;
      } else y += 100; // death
    } else {
      // set sonic sprite and remove upwards speed in case it landed this frame
      walkable_down = is_walkable(x, y + 1);
      if (!walkable_down) sonic_obj = set_sprite(sonic_obj, SONIC) & ~SE_VFLIP;
    }
    set_tile(x, y, sonic_obj);

    // tile break, to allow more exploring
    is_looking_left = sonic_obj & SE_HFLIP;
    if (key_hit(KEY_B)) set_tile(x + (is_looking_left ? -1 : 1), y, EMPTY);
	}

	return 0;
}
