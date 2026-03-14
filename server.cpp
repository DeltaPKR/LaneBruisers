#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <iostream>
#include <chrono>
#include <algorithm>

#define PORT 54000
#define MAX_EVENTS 64

struct Unit
{
    float x;
    float hp;   // float so fractional dmg*dt works correctly
    int   dmg;
    float speed;
};

// How long (seconds) a player must wait between spawning each unit type
static const float SPAWN_COOLDOWN[5] = { 1.5f, 2.0f, 2.5f, 3.0f, 3.5f };

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

struct Player
{
    int   id;
    int   fd;
    float coins = 15.f;
    int   baseHP = 100;
    bool  connected = true;
    int   matchID = -1;

    // Last time each unit type was successfully spawned (epoch == never)
    TimePoint lastSpawn[5] = {};
};

struct Match
{
    int id;
    Player* p1;
    Player* p2;
    std::vector<Unit> u1;
    std::vector<Unit> u2;
    bool running = true;
};

std::vector<Player*> lobby;
std::unordered_map<int, Player*> playersByFD;
std::unordered_map<int, Player*> playersByID;
std::unordered_map<int, Match> matches;

int nextPlayerID = 1;
int nextMatchID = 1;

int setNonBlocking(int fd)
{
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

void sendMsg(int fd, const std::string& msg)
{
    send(fd, msg.c_str(), msg.size(), 0);
}

void sendState(Match& m)
{
    auto sendTo = [&](Player* p, Player* e,
        std::vector<Unit>& mine,
        std::vector<Unit>& enemy)
        {
            if (!p->connected) return;

            std::string msg =
                "STATE " +
                std::to_string((int)p->coins) + " " +
                std::to_string((int)e->coins) + " " +
                std::to_string(p->baseHP) + " " +
                std::to_string(e->baseHP);

            msg += " U1 " + std::to_string(mine.size());
            for (auto& u : mine)
                msg += " " + std::to_string((int)u.x) + " " + std::to_string((int)u.hp);

            msg += " U2 " + std::to_string(enemy.size());
            for (auto& u : enemy)
                msg += " " + std::to_string((int)u.x) + " " + std::to_string((int)u.hp);

            msg += "\n";
            sendMsg(p->fd, msg);
        };

    sendTo(m.p1, m.p2, m.u1, m.u2);
    sendTo(m.p2, m.p1, m.u2, m.u1);
}

void updateMatch(Match& m, float dt)
{
    if (!m.running) return;

    // coins is now float � this actually accumulates properly
    m.p1->coins += 2.f * dt;
    m.p2->coins += 2.f * dt;

    for (auto& u : m.u1) u.x += u.speed * dt;
    for (auto& u : m.u2) u.x -= u.speed * dt;

    // Combat: apply damage proportional to dt so tick rate doesnt matter
    for (auto& a : m.u1)
        for (auto& b : m.u2)
        {
            if (std::abs(a.x - b.x) < 20.f)
            {
                a.hp -= (float)b.dmg * dt * 10.f;
                b.hp -= (float)a.dmg * dt * 10.f;
            }
        }

    auto removeDead = [](std::vector<Unit>& v)
        {
            v.erase(std::remove_if(v.begin(), v.end(),
                [](const Unit& u) { return u.hp <= 0.f; }), v.end());
        };

    size_t before1 = m.u1.size();
    size_t before2 = m.u2.size();

    removeDead(m.u1);
    removeDead(m.u2);

    if (before1 > m.u1.size()) m.p2->coins += 5.f;
    if (before2 > m.u2.size()) m.p1->coins += 5.f;

    // units that reach the base are removed after dealing damage so they dont drain baseHP every tick forever
    auto& u1 = m.u1;
    auto& u2 = m.u2;

    for (auto& u : u1)
        if (u.x >= 750.f) m.p2->baseHP -= u.dmg;

    for (auto& u : u2)
        if (u.x <= 50.f) m.p1->baseHP -= u.dmg;

    // Remove units that reached the opposite base
    u1.erase(std::remove_if(u1.begin(), u1.end(),
        [](const Unit& u) { return u.x >= 750.f; }), u1.end());

    u2.erase(std::remove_if(u2.begin(), u2.end(),
        [](const Unit& u) { return u.x <= 50.f; }), u2.end());

    if (m.p1->baseHP <= 0)
    {
        sendMsg(m.p1->fd, "LOSE\n");
        sendMsg(m.p2->fd, "WIN\n");
        m.running = false;
        return;
    }

    if (m.p2->baseHP <= 0)
    {
        sendMsg(m.p1->fd, "WIN\n");
        sendMsg(m.p2->fd, "LOSE\n");
        m.running = false;
        return;
    }

    sendState(m);
}

void tryMatch()
{
    while (lobby.size() >= 2)
    {
        Player* a = lobby[0];
        Player* b = lobby[1];

        Match m;
        m.id = nextMatchID++;
        m.p1 = a;
        m.p2 = b;

        a->matchID = m.id;
        b->matchID = m.id;

        matches[m.id] = m;

        // tell each player their side and the server-enforced cooldowns
        std::string cooldownStr;
        for (int i = 0; i < 5; i++)
            cooldownStr += " " + std::to_string(SPAWN_COOLDOWN[i]);

        sendMsg(a->fd, "START 1" + cooldownStr + "\n");
        sendMsg(b->fd, "START 2" + cooldownStr + "\n");

        lobby.erase(lobby.begin(), lobby.begin() + 2);
    }
}

void spawnUnit(Player* p, int type)
{
    if (!p || p->matchID == -1) return;
    if (type < 0 || type > 4) return;

    Match& m = matches[p->matchID];
    if (!m.running) return;

    const int cost[5] = { 5, 7, 10, 12, 15 };
    if (p->coins < cost[type]) return;

    // Server-side cooldown � reject the request if not enough time has passed
    auto now = Clock::now();
    float elapsed = std::chrono::duration<float>(now - p->lastSpawn[type]).count();
    if (elapsed < SPAWN_COOLDOWN[type]) return;

    p->coins -= cost[type];
    p->lastSpawn[type] = now;

    Unit u;
    u.hp = 20.f + type * 10.f;
    u.dmg = 2 + type * 2;
    u.speed = 60.f;

    // compare pointer identity to decide which side the player is on
    if (m.p1 == p)
    {
        u.x = 60.f;
        m.u1.push_back(u);
    }
    else if (m.p2 == p)
    {
        u.x = 740.f;
        m.u2.push_back(u);
    }
}

// Line-buffer per file descriptor so we handle partial/split TCP messages if needed idk yet
std::unordered_map<int, std::string> recvBuf;

void processLine(int fd, const std::string& line)
{
    if (line.empty()) return;

    if (line.rfind("HELLO", 0) == 0 && line.size() > 6)
    {
        // Reconnecting player identifies itself by its assigned ID (dont have reconnect yet but will ad it maybe idk if this works)
        int id = std::stoi(line.substr(6));
        if (playersByID.count(id))
        {
            Player* p = playersByID[id];
            // Clean up old fd mapping
            playersByFD.erase(p->fd);
            p->fd = fd;
            p->connected = true;
            playersByFD[fd] = p;
        }
        return;
    }

    if (line.rfind("SPAWN", 0) == 0 && line.size() > 6)
    {
        int type = line[6] - '0';
        if (playersByFD.count(fd))
            spawnUnit(playersByFD[fd], type);
        return;
    }
}

int main()
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listen_fd, (sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, SOMAXCONN);
    setNonBlocking(listen_fd);

    int epoll_fd = epoll_create1(0);
    epoll_event ev{ EPOLLIN, {.fd = listen_fd} };
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    epoll_event events[MAX_EVENTS];
    auto last = std::chrono::steady_clock::now();

    while (true)
    {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, 10);

        for (int i = 0; i < n; i++)
        {
            int fd = events[i].data.fd;

            if (fd == listen_fd)
            {
                int cfd = accept(listen_fd, nullptr, nullptr);
                setNonBlocking(cfd);

                epoll_event cev{ EPOLLIN, {.fd = cfd} };
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cfd, &cev);

                Player* p = new Player();
                p->id = nextPlayerID++;
                p->fd = cfd;

                playersByFD[cfd] = p;
                playersByID[p->id] = p;

                // send the player their ID so the client can use HELLO on reconnect
                sendMsg(cfd, "ID " + std::to_string(p->id) + "\n");
                sendMsg(cfd, "WAITING\n");

                lobby.push_back(p);
                tryMatch();
            }
            else
            {
                char buf[4096] = { 0 };
                int  r = recv(fd, buf, sizeof(buf) - 1, 0);

                if (r <= 0)
                {
                    if (playersByFD.count(fd))
                        playersByFD[fd]->connected = false;
                    recvBuf.erase(fd);
                    close(fd);
                    continue;
                }

                // accumulate data and process complete newline-terminated messages
                recvBuf[fd] += std::string(buf, r);
                auto& rb = recvBuf[fd];
                size_t pos;
                while ((pos = rb.find('\n')) != std::string::npos)
                {
                    std::string line = rb.substr(0, pos);
                    rb.erase(0, pos + 1);
                    processLine(fd, line);
                }
            }
        }

        auto  now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();

        if (dt > 0.05f)
        {
            for (auto& [id, m] : matches)
                updateMatch(m, dt);
            last = now;
        }
    }
}