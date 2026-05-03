#include "boardspec.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static int countMineNeighbours(const std::vector<bool>& mines, int width, int height, int col, int row)
{
    static const int dx[] = {-1, 0, 1, 1,  1,  0, -1, -1};
    static const int dy[] = {-1,-1,-1, 0,  1,  1,  1,  0};
    int count = 0;
    for (int d = 0; d < 8; ++d)
    {
        int nc = col + dx[d];
        int nr = row + dy[d];
        if (nc >= 0 && nc < width && nr >= 0 && nr < height)
            count += mines[nr * width + nc] ? 1 : 0;
    }
    return count;
}

std::optional<BoardSpec> loadBoardSpec(const std::string& path)
{
    std::ifstream f(path);
    if (!f)
    {
        std::cerr << "board-file: cannot open '" << path << "'\n";
        return std::nullopt;
    }

    std::vector<std::string> visibleLines;
    std::vector<std::string> mineLines;
    bool blankSeen = false;

    std::string line;
    while (std::getline(f, line))
    {
        if (!blankSeen)
        {
            if (line.empty())
                blankSeen = true;
            else
                visibleLines.push_back(line);
        }
        else
        {
            if (!line.empty())
                mineLines.push_back(line);
        }
    }

    if (visibleLines.empty())
    {
        std::cerr << "board-file: visible section is empty\n";
        return std::nullopt;
    }
    if (mineLines.empty())
    {
        std::cerr << "board-file: mine truth section is missing\n";
        return std::nullopt;
    }
    if (visibleLines.size() != mineLines.size())
    {
        std::cerr << "board-file: visible section has " << visibleLines.size()
                  << " rows but mine section has " << mineLines.size() << " rows\n";
        return std::nullopt;
    }

    int height = static_cast<int>(visibleLines.size());
    int width  = static_cast<int>(visibleLines[0].size());

    for (int row = 0; row < height; ++row)
    {
        if (static_cast<int>(visibleLines[row].size()) != width)
        {
            std::cerr << "board-file: visible row " << row << " has width "
                      << visibleLines[row].size() << ", expected " << width << "\n";
            return std::nullopt;
        }
        if (static_cast<int>(mineLines[row].size()) != width)
        {
            std::cerr << "board-file: mine row " << row << " has width "
                      << mineLines[row].size() << ", expected " << width << "\n";
            return std::nullopt;
        }
    }

    int total = width * height;
    BoardSpec spec;
    spec.width  = width;
    spec.height = height;
    spec.mines.resize(total, false);
    spec.states.resize(total, NOT_CLICKED);
    spec.values.resize(total, NA);

    // Parse mine truth map
    for (int row = 0; row < height; ++row)
    {
        for (int col = 0; col < width; ++col)
        {
            char c = mineLines[row][col];
            if (c == '*')
                spec.mines[row * width + col] = true;
            else if (c == '.')
                spec.mines[row * width + col] = false;
            else
            {
                std::cerr << "board-file: unexpected char '" << c
                          << "' in mine section at (" << col << "," << row << ")\n";
                return std::nullopt;
            }
        }
    }

    // Parse visible board
    for (int row = 0; row < height; ++row)
    {
        for (int col = 0; col < width; ++col)
        {
            int idx = row * width + col;
            char c = visibleLines[row][col];
            if (c == '#')
            {
                spec.states[idx] = NOT_CLICKED;
                spec.values[idx] = NA;
            }
            else if (c == 'F')
            {
                spec.states[idx] = FLAG_CLICKED;
                spec.values[idx] = NA;
            }
            else if (c == ' ')
            {
                spec.states[idx] = CLICKED;
                spec.values[idx] = EMPTY;
            }
            else if (c >= '1' && c <= '8')
            {
                spec.states[idx] = CLICKED;
                spec.values[idx] = c - '0';
            }
            else
            {
                std::cerr << "board-file: unexpected char '" << c
                          << "' in visible section at (" << col << "," << row << ")\n";
                return std::nullopt;
            }
        }
    }

    // Cross-validate: each CLICKED numbered cell must match the truth map
    for (int row = 0; row < height; ++row)
    {
        for (int col = 0; col < width; ++col)
        {
            int idx = row * width + col;
            if (spec.states[idx] == CLICKED && spec.values[idx] >= 1 && spec.values[idx] <= 8)
            {
                int expected = countMineNeighbours(spec.mines, width, height, col, row);
                if (expected != spec.values[idx])
                {
                    std::cerr << "board-file: cell (" << col << "," << row
                              << ") shows " << spec.values[idx]
                              << " but truth map has " << expected << " mine neighbours\n";
                    return std::nullopt;
                }
            }
        }
    }

    return spec;
}
