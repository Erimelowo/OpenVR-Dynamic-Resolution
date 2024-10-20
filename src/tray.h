#ifndef TRAY_H
#define TRAY_H

#ifdef __cplusplus
extern "C"
{
#endif

struct tray {
  const char *icon_filepath;
  const char *tooltip;
  void (*cb)(struct tray *); // called on left click, leave null to just open menu
  struct tray_menu_item *menu;
};

struct tray_menu_item {
  const char *text;
  int disabled;
  int checked;
  void (*cb)(struct tray_menu_item *);
  struct tray_menu_item *submenu;
};

struct tray * tray_get_instance();

int tray_init(struct tray *tray);

int tray_loop(int blocking);

void tray_update(struct tray *tray);

void tray_exit(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* TRAY_H */
