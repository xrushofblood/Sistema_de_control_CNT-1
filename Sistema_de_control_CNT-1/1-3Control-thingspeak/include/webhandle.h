#ifndef WEBHANDLE_H 
#define WEBHANDLE

// Run modes
#define RUN_AS_AP  0
#define RUN_AS_STA 1

void cache_web_content(int run_mode);
void handleRoot_ap();
void handleRoot_sta();
void handleNotFound();

#endif