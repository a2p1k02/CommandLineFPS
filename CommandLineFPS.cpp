#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <chrono>
#include <ncurses.h>
#include <sstream>

using namespace std;

int nScreenWidth = 120;       // Console Screen Size X (columns)
int nScreenHeight = 40;       // Console Screen Size Y (rows)
int nMapWidth = 16;           // World Dimensions
int nMapHeight = 16;

float fPlayerX = 14.7f;       // Player Start Position
float fPlayerY = 5.09f;
float fPlayerA = 0.0f;        // Player Start Rotation
float fFOV = 3.14159f / 4.0f; // Field of View
float fDepth = 16.0f;         // Maximum rendering distance
float fSpeed = 150.0f;        // Walking Speed

int main()
{
    // NCurses setup
    setlocale(LC_ALL, "");    // Set locale for UTF-8 support
    initscr();                // Initialise NCurses screen
    noecho();                 // Don't echo input to screen
    curs_set(0);              // Don't show terminal cursor
    nodelay(stdscr, true);    // Don't halt program while waiting for input
    cbreak();                 // Make input characters immediately available to the program

    // Create Screen Buffer
    auto *screen = new wchar_t[nScreenWidth * nScreenHeight];

    // Create Map of world space # = wall block, . = space
    wstring map;
    map += L"#########.......";
    map += L"#...............";
    map += L"#.......########";
    map += L"#..............#";
    map += L"#......##......#";
    map += L"#......##......#";
    map += L"#..............#";
    map += L"###............#";
    map += L"##.............#";
    map += L"#......####..###";
    map += L"#......#.......#";
    map += L"#......#.......#";
    map += L"#..............#";
    map += L"#......#########";
    map += L"#..............#";
    map += L"################";

    auto tp1 = chrono::system_clock::now();
    auto tp2 = chrono::system_clock::now();

    bool finished = false;
    while (!finished)
    {
        // We'll need time differential per frame to calculate modification
        // to movement speeds, to ensure consistant movement, as ray-tracing
        // is non-deterministic
        tp2 = chrono::system_clock::now();
        chrono::duration<float> elapsedTime = tp2 - tp1;
        tp1 = tp2;
        float fElapsedTime = elapsedTime.count();

        // Measure terminal size
        int terminalWidth;
        int terminalHeight;
        bool cleared = false;
        getmaxyx(stdscr, terminalHeight, terminalWidth);

        // Ensure terminal size is OK
        while (terminalHeight != nScreenHeight || terminalWidth != nScreenWidth)
        {
            if (!cleared)
            {
                clear();
                cleared = true;
            }

            ostringstream out;
            out << "Resize your terminal to (" << nScreenWidth << ", " << nScreenHeight << ") - current size is (" << terminalWidth << ", " << terminalHeight << ")";
            mvaddstr(0, 0, out.str().c_str());
            refresh();
            getmaxyx(stdscr, terminalHeight, terminalWidth);
        }

        int key = getch();
        switch (key) {
            case 'a':
                // CCW Rotation
                fPlayerA -= (fSpeed * 0.75f) * fElapsedTime;
                break;
            case 'd':
                // CW Rotation
                fPlayerA += (fSpeed * 0.75f) * fElapsedTime;
                break;
            case 'w':
                // Forward movement
                fPlayerX += sinf(fPlayerA) * fSpeed * fElapsedTime;;
                fPlayerY += cosf(fPlayerA) * fSpeed * fElapsedTime;;
                if (map.c_str()[(int) fPlayerX * nMapWidth + (int) fPlayerY] == '#') {
                    fPlayerX -= sinf(fPlayerA) * fSpeed * fElapsedTime;;
                    fPlayerY -= cosf(fPlayerA) * fSpeed * fElapsedTime;;
                }
                break;
            case 's':
                // Backward movement
                fPlayerX -= sinf(fPlayerA) * fSpeed * fElapsedTime;;
                fPlayerY -= cosf(fPlayerA) * fSpeed * fElapsedTime;;
                if (map.c_str()[(int) fPlayerX * nMapWidth + (int) fPlayerY] == '#') {
                    fPlayerX += sinf(fPlayerA) * fSpeed * fElapsedTime;;
                    fPlayerY += cosf(fPlayerA) * fSpeed * fElapsedTime;;
                }
                break;
            case 'q':
                // Quit
                finished = true;
                break;
            default:
                break;
        }

        for (int x = 0; x < nScreenWidth; x++)
        {
            // For each column, calculate the projected ray angle into world space
            float fRayAngle = (fPlayerA - fFOV/2.0f) + ((float)x / (float)nScreenWidth) * fFOV;

            // Find distance to wall
            float fStepSize = 0.1f;			// Increment size for ray casting, decrease to increase resolution
            float fDistanceToWall = 0.0f;

            bool bHitWall = false;		// Set when ray hits wall block
            bool bBoundary = false;		// Set when ray hits boundary between two wall blocks

            float fEyeX = sinf(fRayAngle); // Unit vector for ray in player space
            float fEyeY = cosf(fRayAngle);

            // Incrementally cast ray from player, along ray angle, testing for
            // intersection with a block
            while (!bHitWall && fDistanceToWall < fDepth)
            {
                fDistanceToWall += fStepSize;
                int nTestX = (int)(fPlayerX + fEyeX * fDistanceToWall);
                int nTestY = (int)(fPlayerY + fEyeY * fDistanceToWall);

                // Test if ray is out of bounds
                if (nTestX < 0 || nTestX >= nMapWidth || nTestY < 0 || nTestY >= nMapHeight)
                {
                    bHitWall = true;			// Just set distance to maximum depth
                    fDistanceToWall = fDepth;
                }
                else
                {
                    // Ray is inbounds so test to see if the ray cell is a wall block
                    if (map.c_str()[nTestX * nMapWidth + nTestY] == '#')
                    {
                        // Ray has hit wall
                        bHitWall = true;

                        // To highlight tile boundaries, cast a ray from each corner
                        // of the tile, to the player. The more coincident this ray
                        // is to the rendering ray, the closer we are to a tile
                        // boundary, which we'll shade to add detail to the walls
                        vector<pair<float, float>> p;

                        // Test each corner of hit tile, storing the distance from
                        // the player, and the calculated dot product of the two rays
                        for (int tx = 0; tx < 2; tx++)
                            for (int ty = 0; ty < 2; ty++)
                            {
                                // Angle of corner to eye
                                float vy = (float)nTestY + ty - fPlayerY;
                                float vx = (float)nTestX + tx - fPlayerX;
                                float d = sqrt(vx*vx + vy*vy);
                                float dot = (fEyeX * vx / d) + (fEyeY * vy / d);
                                p.push_back(make_pair(d, dot));
                            }

                        // Sort Pairs from closest to farthest
                        sort(p.begin(), p.end(), [](const pair<float, float> &left, const pair<float, float> &right) {return left.first < right.first; });

                        // First two/three are closest (we will never see all four)
                        float fBound = 0.005;
                        if (acos(p.at(0).second) < fBound) bBoundary = true;
                        if (acos(p.at(1).second) < fBound) bBoundary = true;
                        if (acos(p.at(2).second) < fBound) bBoundary = true;
                    }
                }
            }

            // Calculate distance to ceiling and floor
            int nCeiling = (int)((float)(nScreenHeight/2.0) - nScreenHeight / ((float)fDistanceToWall));
            int nFloor = nScreenHeight - nCeiling;

            // Shader walls based on distance
            short nShade = ' ';
            if (fDistanceToWall <= fDepth / 4.0f)			nShade = 0x2588;	// Very close
            else if (fDistanceToWall < fDepth / 3.0f)		nShade = 0x2593;
            else if (fDistanceToWall < fDepth / 2.0f)		nShade = 0x2592;
            else if (fDistanceToWall < fDepth)				nShade = 0x2591;
            else											nShade = ' ';		// Too far away

            if (bBoundary)
                nShade = ' '; // Black it out

            for (int y = 0; y < nScreenHeight; y++)
            {
                // Each Row
                if(y <= nCeiling)
                    screen[y*nScreenWidth + x] = ' ';
                else if(y > nCeiling && y <= nFloor)
                    screen[y*nScreenWidth + x] = nShade;
                else // Floor
                {
                    // Shade floor based on distance
                    float b = 1.0f - (((float)y -nScreenHeight/2.0f) / ((float)nScreenHeight / 2.0f));
                    if (b < 0.25)		nShade = '#';
                    else if (b < 0.5)	nShade = 'x';
                    else if (b < 0.75)	nShade = '.';
                    else if (b < 0.9)	nShade = '-';
                    else				nShade = ' ';
                    screen[y*nScreenWidth + x] = nShade;
                }
            }
        }

        // Display Frame
        mvaddwstr(0, 0, screen);

        // Display Stats
        wchar_t stats[40];
        swprintf(stats, 40, L"X=%3.2f, Y=%3.2f, A=%3.2f FPS=%3.2f ", fPlayerX, fPlayerY, fPlayerA, 1.0f/fElapsedTime);
        mvaddwstr(0, 0, stats);

        // Display Map
        for (int nx = 0; nx < nMapWidth; nx++)
        {
            for (int ny = 0; ny < nMapWidth; ny++)
            {
                mvaddch(ny+1, nx, (chtype)map[ny * nMapWidth + nx]);
            }
        }

        // Display Player
        mvaddch((int)fPlayerX+1, ((int)fPlayerY), 'P');

        // Draw screen
        refresh();
    }

    endwin();
    return 0;
}
