#ifndef WEBHANDLE_H
#define WEBHANDLE_H

// Run modes
#define RUN_AS_STA 1
#define RUN_AS_BT  0


void cache_web_content(int run_mode);
void handleRoot_sta();
void handleNotFound();

#endif
