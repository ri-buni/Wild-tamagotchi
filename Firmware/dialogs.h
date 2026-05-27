#ifndef DIALOGS_H
#define DIALOGS_H

// =====================================================================
// Dialog lines for each pet. Two distinct voices.
// Rabbit = quiet, contemplative, short, lowercase, lots of "..."
// Bunny  = energetic, EXCITED!!, exclamation marks, ALL CAPS sometimes
// =====================================================================

// ---------- RABBIT (quiet) ----------
const char* rabbitIdle[] = {
  "...",
  "Mm.",
  "Quiet day.",
  "*sips tea*",
  "Reading...",
  "Peaceful.",
  "Hello.",
  "*nods*",
  "Nice."
};
const int rabbitIdleCount = sizeof(rabbitIdle) / sizeof(rabbitIdle[0]);

const char* rabbitNight[] = {
  "Goodnight.",
  "Stars are out.",
  "*yawns*",
  "Sleep well."
};
const int rabbitNightCount = sizeof(rabbitNight) / sizeof(rabbitNight[0]);

const char* rabbitEat[]    = { "Mm. Good.",    "Thank you.",  "Tasty." };
const char* rabbitPlay[]   = { "*small hop*",  "Fun.",        "Heh." };
const char* rabbitClean[]  = { "Mmhm.",        "Better.",     "Refreshing." };
const char* rabbitSleep[]  = { "*yawns*",      "Resting...",  "zzz" };
const char* rabbitVisit[]  = { "Hello, friend.","Came to see you.","Is Bunny home?" };
const char* rabbitEvolve[] = { "*grows*",      "I'm bigger.", "Hmm. Changes." };
const char* rabbitMeet[]   = { "Hello, Bunny.","Nice to see you.","*small smile*" };

// Need-driven lines — said when a stat drops low
const char* rabbitHungry[] = { "Hungry...",    "Need food.",  "*small sigh*" };
const char* rabbitTired[]  = { "Sleepy...",    "Need rest.",  "*yawns*" };
const char* rabbitDirty[]  = { "Need a wash.", "Hmm... dusty.","Untidy." };
const char* rabbitBored[]  = { "Lonely.",      "Play... please.","Quiet too long." };

// ---------- BUNNY (energetic) ----------
const char* bunnyIdle[] = {
  "HI HI HI!",
  "WANNA PLAY??",
  "BOING!!",
  "WHEEEE!",
  "HEHEHE!",
  "ZOOM ZOOM!",
  "OH! OH!",
  "JUMP JUMP!",
  "YIPPEE!"
};
const int bunnyIdleCount = sizeof(bunnyIdle) / sizeof(bunnyIdle[0]);

const char* bunnyNight[] = {
  "*loud snore*",
  "STARS!! WOW!!",
  "sleepy zzz",
  "*flops over*"
};
const int bunnyNightCount = sizeof(bunnyNight) / sizeof(bunnyNight[0]);

const char* bunnyEat[]    = { "NOM NOM NOM!!", "YUMMY!!",     "MORE!! MORE!!" };
const char* bunnyPlay[]   = { "WHEEEE!!",      "FUN FUN!!",   "AGAIN!!" };
const char* bunnyClean[]  = { "SO SPARKLY!!",  "SHINY ME!!",  "WOOO CLEAN!!" };
const char* bunnySleep[]  = { "TIRED!!",       "*flops*",     "ZZZ LOUD!!" };
const char* bunnyVisit[]  = { "RABBIT!! HI!!", "I MISSED YOU!!","PLAY PLAY!!" };
const char* bunnyEvolve[] = { "I'M BIGGER!!",  "WOW WOW WOW!!","GROWING!!" };
const char* bunnyMeet[]   = { "FRIEND!! HI!!", "YAY OUTSIDE!!","LET'S RUN!!" };

// Need-driven lines
const char* bunnyHungry[] = { "FEED ME!!",     "TUMMY GROWLS!!","HUNGRY HUNGRY!!" };
const char* bunnyTired[]  = { "SLEEPY EYES!!", "NAP TIME PLZ!!","*flop*" };
const char* bunnyDirty[]  = { "ICKY ME!!",     "BATH PLEASE!!", "STICKY!!" };
const char* bunnyBored[]  = { "BORED!! BORED!!","PLAY PLAY PLAY!!","I MISS FUN!!" };

// Pick a random line from a string array of given size
const char* pickLine(const char** lines, int count) {
  return lines[random(count)];
}

#endif
