/*
    MIT License

    Copyright (c) 2022 KeAcquireSpinLock

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include <fmt/format.h>
#include <fmt/color.h>

#include <cassert>
#include <iostream>
#include <regex>
#include <set>
#include <fstream>
#include <thread>

struct Vector
{
    int x;
    int y;

    constexpr Vector(int x, int y) : x(x), y(y) {}
    constexpr Vector() : Vector(0, 0) { }

    Vector operator+(const Vector &other) const
    {
        return Vector(x + other.x, y + other.y);
    }

    Vector operator-(const Vector &other) const
    {
        return Vector(x - other.x, y - other.y);
    }

    Vector operator*(const Vector &other) const
    {
        return Vector(x * other.x, y * other.y);
    }

    Vector operator*(int factor) const
    {
        return Vector(x * factor, y * factor);
    }

    bool operator==(const Vector &other) const
    {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Vector &other) const
    {
        return !(*this == other);
    }

    bool operator<(const Vector &other) const
    {
        return x < other.x && y < other.y; // doesnt really matter
    }
};

namespace Direction
{
enum
{
    UL = 0,
    UR,
    DL,
    DR,
    MAX
};
}

class Board
{
public:
    Board()
        : mBoard(SIZE * SIZE, EMPTY)
    {
        for (int y = 0; y < 3; y++)
            for (int x = y % 2; x < SIZE; x += 2)
                (*this)(x, y) = WHITE;

        for (int y = SIZE - 3; y < SIZE; y++)
            for (int x = y % 2; x < SIZE; x += 2)
                (*this)(x, y) = BLACK;
    }

public:
    char &operator()(int x, int y)
    {
        assert(x >= 0 && y >= 0);
        return mBoard[y * SIZE + x];
    }

    const char &operator()(int x, int y) const
    {
        assert(x >= 0 && y >= 0);
        return mBoard[y * SIZE + x];
    }

    char &operator[](const Vector &pos)
    {
        return operator()(pos.x, pos.y);
    }

    const char &operator[](const Vector &pos) const
    {
        return operator()(pos.x, pos.y);
    }

public:
    void Print() const
    {
        fmt::print(fmt::bg(fmt::color::dark_gray) | fmt::fg(fmt::color::red), " O ");
        fmt::print(": {}\n", mWhiteCaptured);

        fmt::print(fmt::bg(fmt::color::saddle_brown) | fmt::fg(fmt::color::black), " O ");
        fmt::print(": {}\n\n", mBlackCaptured);

        std::string header(SIZE * 3, ' ');
        char first = 'A';
        for (size_t i = 0; i < header.size(); i += 3)
            header[i] = first++;

        fmt::print("   {}\n", header);
        for (int y = 0; y < SIZE; y++)
        {
            fmt::print("{} ", SIZE - y);
            for (int x = 0; x < SIZE; x++)
            {
                const auto bg = fmt::bg((y % 2 == 0 && x % 2 == 1) || (y % 2 == 1 && x % 2 == 0) 
                    ? fmt::color::saddle_brown : fmt::color::dark_gray) | fmt::emphasis::bold;
                switch ((*this)(x, y))
                {
                case WHITE:
                    fmt::print(bg | fmt::fg(fmt::color::red), " O ");
                    break;
                case BLACK:
                    fmt::print(bg | fmt::fg(fmt::color::black), " O ");
                    break;
                case WHITE_KING:
                    fmt::print(bg | fmt::fg(fmt::color::red), " K ");
                    break;
                case BLACK_KING:
                    fmt::print(bg | fmt::fg(fmt::color::black), " K ");
                    break;
                case ' ':
                    fmt::print(bg, "   ");
                    break;
                }
            }
            fmt::print("\n");
        }
    }

    int GetVictor() const
    {
        return mVictor;
    }

    int GetCurrentTurn() const
    {
        return mTurn;
    }

    void Move(const Vector &pos, int dir, int count)
    {
        ValidateMove(pos, dir, count, true, mTurn);

        Vector end = pos + DIR[dir] * count;

        (*this)[end] = (*this)[pos];
        (*this)[pos] = EMPTY;

        if (count == 2)
        {
            (*this)[pos + DIR[dir]] = EMPTY;
            if (mTurn == PLAYERW)
                mBlackCaptured++;
            else
                mWhiteCaptured++;
        }          

        if (end.y == 0 && mTurn == PLAYERB)
            (*this)[end] = BLACK_KING;
        
        if (end.y == SIZE - 1 && mTurn == PLAYERW)
            (*this)[end] = WHITE_KING;

        // If the player has another jump they can make, they must make it
        auto jumps = GetJumps(end);
        if (count == 2 && jumps.size())
        {
            mForcedJumps = jumps;
            return;
        } else {
            mForcedJumps.clear();
        }
        
        mTurn = mTurn == PLAYERB ? PLAYERW : PLAYERB;
        if (!HasValidMoves(mTurn))
            mVictor = mTurn == PLAYERB ? PLAYERW : PLAYERB;
    }

    std::vector<std::tuple<Vector, int, int>> GetValidMoves(int player) const
    {
        std::vector<std::tuple<Vector, int, int>> moves;
        for (int y = 0; y < SIZE; y++)
        {
            for (int x = 0; x < SIZE; x++)
            {
                const Vector &pos{x, y};
                if (GetPlayerForTile(pos) != player)
                    continue;

                for (int i = 0; i < Direction::MAX; i++)
                {
                    for (int c = 1; c < 3; c++)
                    {
                        try
                        {
                            ValidateMove(pos, i, c, true, player);
                            moves.push_back(std::make_tuple(pos, i, c));
                        } catch (const std::invalid_argument &)
                        {
                            continue;
                        }
                    }
                }
            }
        }

        return moves;
    }

    bool HasValidMoves(int player) const
    {
        return GetValidMoves(player).size() > 0;
    }

    std::string ToNotation(const Vector &v) const
    {
        return std::string({static_cast<char>('A' + v.x), static_cast<char>('8' - v.y)}); // only works with size 8
    }

private:
    bool IsInBounds(const Vector &pos) const
    {
        return pos.x >= 0 && pos.y >= 0 && pos.x < SIZE && pos.y < SIZE;
    }

    void ValidateMove(const Vector &pos, int dir, int count, bool jump, int player) const
    {
        if (!IsInBounds(pos))
            throw std::invalid_argument("Start position is out of bounds.");

        if (GetPlayerForTile(pos) != player)
            throw std::invalid_argument("Cannot move this piece.");

        if (count > 2 || count < 1 || dir < 0 || dir >= Direction::MAX)
            throw std::invalid_argument("Movement is invalid.");

        Vector end = pos + DIR[dir] * count;
        if (!IsInBounds(end) || (*this)[end] != EMPTY)
            throw std::invalid_argument("End position is invalid.");

        if (count == 2 && GetPlayerForTile(pos + DIR[dir]) != (player == PLAYERW ? PLAYERB : PLAYERW))
            throw std::invalid_argument("Cannot make this jump.");

        if (jump)
        {
            std::set<Vector> jumps{mForcedJumps};
            if (mForcedJumps.empty())
            {
                for (int y = 0; y < SIZE; y++)
                {
                    for (int x = 0; x < SIZE; x++)
                    {
                        Vector tmp{x, y};
                        if (GetPlayerForTile(tmp) != player)
                            continue;

                        auto jump = GetJumps(tmp);
                        jumps.insert(jump.begin(), jump.end());
                    }
                }
            }

            if (!jumps.empty())
            {
                auto it = std::find(jumps.begin(), jumps.end(), pos + DIR[dir]);
                if (it == jumps.end() || count != 2)
                {
                    std::string result;
                    for (const auto &jump : jumps)
                        result += ToNotation(jump) + ", ";
                    throw std::invalid_argument(fmt::format("Must jump over (one of) {}.", result.substr(0, result.size() - 2)));
                }
            }
        }

        switch ((*this)[pos])
        {
        case WHITE:
            if (dir != Direction::DL && dir != Direction::DR)
                throw std::invalid_argument("Movement is invalid for red.");

            break;
        case BLACK:
            if (dir != Direction::UL && dir != Direction::UR)
                throw std::invalid_argument("Movement is invalid for black.");
            
            break;
        }
    }

    std::set<Vector> GetJumps(const Vector &pos) const
    {
        std::set<Vector> jumps;
        for (int dir = 0; dir < Direction::MAX; dir++)
        {
            try
            {
                ValidateMove(pos, dir, 2, false, mTurn);
                jumps.insert(pos + DIR[dir]);
            } catch (const std::invalid_argument &) { continue; }                
        }

        return jumps;
    }

    int GetPlayerForTile(const Vector &pos) const
    {
        switch ((*this)[pos])
        {
        case WHITE:
        case WHITE_KING:
            return PLAYERW;
        case BLACK:
        case BLACK_KING:
            return PLAYERB;
        default:
            return -1;
        }
    }

public:
    static constexpr const int SIZE        = 8;
    static constexpr const int PLAYERB     = 0;
    static constexpr const int PLAYERW     = 1;

    static constexpr const char WHITE      = 'W';
    static constexpr const char BLACK      = 'B';
    static constexpr const char WHITE_KING = 'K';
    static constexpr const char BLACK_KING = 'X';
    static constexpr const char EMPTY      = ' ';

    static constexpr const Vector DIR[Direction::MAX] = { {-1, -1}, {1, -1}, {-1, 1}, {1, 1} };

private:
    std::set<Vector> mForcedJumps;
    std::string      mBoard;
    int              mTurn          = PLAYERB;
    int              mVictor        = -1;
    int              mBlackCaptured = 0;
    int              mWhiteCaptured = 0;
};

void ParseInput(const std::string &src, Vector &start, Vector &end, int &d, int &count)
{
    std::smatch match;
    if (!std::regex_match(src, match, std::regex(R"((\w\d) *(?:to)? *(\w\d))", std::regex::icase)))
        throw std::invalid_argument("Command could not be parsed.");

    static auto FromNotation = [] (const std::string &str)
    {
        if (str[0] >= 'A' && str[0] <= 'Z')
            return Vector{str[0] - 'A', '8' - str[1]};
        return Vector{str[0] - 'a', '8' - str[1]};
    };

    start = FromNotation(match.str(1));
    end   = FromNotation(match.str(2));
    Vector diff = end - start;
    if (abs(diff.x) != abs(diff.y) || diff.x == 0)
        throw std::invalid_argument("Invalid position.");

    count = abs(diff.x);

    Vector dir{diff.x / abs(diff.x), diff.y / abs(diff.x)};
    for (d = 0; d < Direction::MAX; d++)
        if (Board::DIR[d] == dir)
            return;

    throw std::invalid_argument("Command could not be parsed.");
}

int main(int argc, char **argv)
{
    std::ifstream file;
    std::ofstream replay;

    if (argc > 1)
        file.open(argv[1]);

    if (argc > 2)
        replay.open(argv[2]);

    std::istream &stream = file.is_open() ? file : std::cin;
    std::string   status;
    Board         board;

    while (1337)
    {
        fmt::print("\033[H\033[J{}\n\n", status);
        status.clear();
        board.Print();
        if (board.GetVictor() != -1)
            break;

        if (!file.is_open())
            fmt::print("\nSelect for {} (ex. B3 to C4): ", board.GetCurrentTurn() == Board::PLAYERB ? "black" : "red");

        std::string line;
        if (!std::getline(stream, line))
        {
            fmt::print("Input stream is invalid.");
            return 1;
        }
        
        try
        {
            Vector start, end;
            int dir, count;
            ParseInput(line, start, end, dir, count);

            status = fmt::format("({} to {}) ", board.ToNotation(start), board.ToNotation(end));
            board.Move(start, dir, count);

            if (replay.good())
                replay << board.ToNotation(start) << board.ToNotation(end) << "\n";

            if (file.is_open())
                std::this_thread::sleep_for(std::chrono::milliseconds{750});
        } catch (const std::invalid_argument &ex)
        {
            status = fmt::format("Invalid command: {}", ex.what());
        }
    }

    fmt::print("\n{} wins!\n", board.GetVictor() == Board::PLAYERB ? "Black" : "Red");
    return 0;
}