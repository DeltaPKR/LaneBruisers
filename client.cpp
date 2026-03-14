#include <SFML/Graphics.hpp>
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <string>

#define SERVER_IP "127.0.0.1"
#define PORT 54000

struct Unit { float x; int hp; };

//centers an sf::Text horizontally inside a given container width
auto centerText = [](sf::Text& t, float containerWidth, float y)  
    {                                                                  
        sf::FloatRect b = t.getLocalBounds();                          
        t.setOrigin(b.left + b.width / 2.f, b.top);                   
        t.setPosition(containerWidth / 2.f, y);                       
    };                                                                 

int main()
{
    sf::RenderWindow window(sf::VideoMode(800, 600), "LaneBruisers");
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.loadFromFile("arial.ttf"))
    {
        std::cerr << "Could not load arial.ttf\n";
        return 1;
    }

    // MAIN MENU
    sf::Text titleText;                                                    
    titleText.setFont(font);                                              
    titleText.setCharacterSize(72);                                        
    titleText.setFillColor(sf::Color(255, 200, 50));                      
    titleText.setStyle(sf::Text::Bold);                                   
    titleText.setString("LaneBruisers");                                  
    centerText(titleText, 800, 110);                                      

    sf::Text subtitleText;                                                
    subtitleText.setFont(font);                                           
    subtitleText.setCharacterSize(20);                                   
    subtitleText.setFillColor(sf::Color(180, 180, 180));                  
    subtitleText.setString("Real-time Multiplayer Lane Battle");           
    centerText(subtitleText, 800, 205);                                   

    sf::RectangleShape menuSep({ 500, 2 });                                  
    menuSep.setFillColor(sf::Color(255, 200, 50, 100));                    
    menuSep.setPosition(150, 255);                                         

    // returns a filled rect + centered label             
    auto makeButton = [&](const std::string& label, float y,               
        sf::Color fill)                                 
        -> std::pair<sf::RectangleShape, sf::Text>                         
        {                                                                  
            sf::RectangleShape rect({ 200, 55 });                      
            rect.setPosition(300, y);                                      
            rect.setFillColor(fill);                                   
            rect.setOutlineThickness(2);                                 
            rect.setOutlineColor(sf::Color(255, 255, 255, 50));                 

            sf::Text txt;                                                   
            txt.setFont(font);                                              
            txt.setCharacterSize(26);                                   
            txt.setFillColor(sf::Color::White);                                
            txt.setStyle(sf::Text::Bold);                             
            txt.setString(label);                                              
            sf::FloatRect b = txt.getLocalBounds();                            
            txt.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);  
            txt.setPosition(400, y + 27);                                     
            return { rect, txt };                                               
        };                                                                     

    auto [playRect, playLabel] = makeButton("PLAY", 295, sf::Color(40, 120, 40));  
    auto [quitRect, quitLabel] = makeButton("QUIT", 370, sf::Color(120, 40, 40));  

    sf::Text hintText;                                             
    hintText.setFont(font);                                                
    hintText.setCharacterSize(15);                                        
    hintText.setFillColor(sf::Color(100, 100, 100));                  

    bool inMenu = true;                                                   
    while (inMenu && window.isOpen())                                      
    {                                                                     
        sf::Vector2i mouse = sf::Mouse::getPosition(window);               

        bool hoverPlay = playRect.getGlobalBounds().contains(             
            (float)mouse.x, (float)mouse.y);             
        bool hoverQuit = quitRect.getGlobalBounds().contains(             
            (float)mouse.x, (float)mouse.y);             

        // bright button when hovered                                   
        playRect.setFillColor(hoverPlay ? sf::Color(70, 180, 70)            
            : sf::Color(40, 120, 40));          
        quitRect.setFillColor(hoverQuit ? sf::Color(180, 70, 70)            
            : sf::Color(120, 40, 40));          

        sf::Event me;                                                      
        while (window.pollEvent(me))                                       
        {                                                                  
            if (me.type == sf::Event::Closed) window.close();             
            if (me.type == sf::Event::KeyPressed &&                        
                me.key.code == sf::Keyboard::Escape) window.close();      
            if (me.type == sf::Event::MouseButtonReleased &&               
                me.mouseButton.button == sf::Mouse::Left)                  
            {                                                              
                if (hoverPlay) inMenu = false;                              
                if (hoverQuit) window.close();                             
            }                                                              
        }                                                                  

        window.clear(sf::Color(18, 18, 28));                                
        window.draw(titleText);                                            
        window.draw(subtitleText);                                         
        window.draw(menuSep);                                              
        window.draw(playRect);   window.draw(playLabel);                   
        window.draw(quitRect);   window.draw(quitLabel);                   
        window.draw(hintText);                                             
        window.display();                                                  
    }                                                                      

    if (!window.isOpen()) return 0;

    int sock = socket(AF_INET, SOCK_STREAM, 0);                    
    sockaddr_in serv{};                                                   
    serv.sin_family = AF_INET;                                            
    serv.sin_port = htons(PORT);                                         
    inet_pton(AF_INET, SERVER_IP, &serv.sin_addr);                         
    connect(sock, (sockaddr*)&serv, sizeof(serv));                         
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);              

    sf::Text infoText;
    infoText.setFont(font);
    infoText.setCharacterSize(20);
    infoText.setPosition(10, 10);
    infoText.setFillColor(sf::Color::White);

    sf::Text statusText;
    statusText.setFont(font);
    statusText.setCharacterSize(28);
    statusText.setFillColor(sf::Color::Yellow);
    statusText.setPosition(250, 270);

    sf::RectangleShape base1({ 40, 120 });
    base1.setPosition(0, 240);
    base1.setFillColor(sf::Color::Blue);

    sf::RectangleShape base2({ 40, 120 });
    base2.setPosition(760, 240);
    base2.setFillColor(sf::Color::Red);

    // HP bar background
    sf::RectangleShape hpBg({ 200, 18 });
    hpBg.setFillColor(sf::Color(80, 0, 0));

    sf::RectangleShape hpBar({ 200, 18 });
    hpBar.setFillColor(sf::Color::Green);

    int  myPlayerID = 0;   //assigned by server, used for HELLO on reconnect
    int  mySide = 0;   // 1 or 2, tells us whose unit list is "mine"
    int  coins = 0;
    int  baseHP = 100;
    int  enemyBaseHP = 100;

    std::vector<Unit> myUnits;
    std::vector<Unit> enemyUnits;

    // Per-key cooldown driven by values the server sends in START
    sf::Clock keyClock[5];
    float     KEY_COOLDOWN[5] = { 1.5f, 2.0f, 2.5f, 3.0f, 3.5f }; // defaults, overwritten by server

    std::string recvBuf;               // accumulate partial TCP data
    std::string gameStatus;            // "WAITING", "WIN", "LOSE", ""
    bool gameOver = false;

    auto processLine = [&](const std::string& line)
        {
            if (line.empty()) return;

            std::istringstream ss(line);
            std::string type;
            ss >> type;

            if (type == "ID")
            {
                // Server tells us our player ID; store it for reconnection
                ss >> myPlayerID;
                // Send HELLO so server can remap our fd if we reconnected
                std::string hello = "HELLO " + std::to_string(myPlayerID) + "\n";
                send(sock, hello.c_str(), hello.size(), 0);
            }
            else if (type == "WAITING")
            {
                gameStatus = "Waiting for opponent...";
            }
            else if (type == "START")
            {
                ss >> mySide;
                // Server also sends the 5 cooldown values
                for (int i = 0; i < 5; i++)
                    if (!(ss >> KEY_COOLDOWN[i])) break;
                gameStatus = "";
            }
            else if (type == "STATE")
            {
                int enemyCoins;
                ss >> coins >> enemyCoins >> baseHP >> enemyBaseHP;

                std::string tag;
                int count;

                // Server always sends U1 (p1 units) then U2 (p2 units).
                std::vector<Unit> u1list, u2list;

                ss >> tag >> count;   // "U1" <n>
                for (int i = 0; i < count; i++)
                {
                    Unit u; ss >> u.x >> u.hp;
                    u1list.push_back(u);
                }
                ss >> tag >> count;   // "U2" <n>
                for (int i = 0; i < count; i++)
                {
                    Unit u; ss >> u.x >> u.hp;
                    u2list.push_back(u);
                }

                if (mySide == 1)
                {
                    myUnits = u1list;
                    enemyUnits = u2list;
                }
                else
                {
                    myUnits = u2list;
                    enemyUnits = u1list;
                }
            }
            else if (type == "WIN")
            {
                gameStatus = "YOU WIN!";
                gameOver = true;
            }
            else if (type == "LOSE")
            {
                gameStatus = "YOU LOSE!";
                gameOver = true;
            }
        };

    while (window.isOpen())
    {
        sf::Event e;
        while (window.pollEvent(e))
            if (e.type == sf::Event::Closed) window.close();

        // Only read keyboard when this window is focused.
        // Without this, both clients on the same machine react to every keypress
        // and mirror each other's spawns exactly.
        if (!gameOver && mySide != 0 && window.hasFocus())
        {
            const sf::Keyboard::Key keys[5] = {
                sf::Keyboard::Num1, sf::Keyboard::Num2, sf::Keyboard::Num3,
                sf::Keyboard::Num4, sf::Keyboard::Num5
            };
            for (int i = 0; i < 5; i++)
            {
                if (sf::Keyboard::isKeyPressed(keys[i]) &&
                    keyClock[i].getElapsedTime().asSeconds() >= KEY_COOLDOWN[i])
                {
                    std::string cmd = "SPAWN " + std::to_string(i) + "\n";
                    send(sock, cmd.c_str(), cmd.size(), 0);
                    keyClock[i].restart();
                }
            }
        }

        // read all available data into buffer, then process complete lines
        char buf[4096];
        int  r;
        while ((r = recv(sock, buf, sizeof(buf) - 1, 0)) > 0)
        {
            recvBuf.append(buf, r);
        }

        size_t pos;
        while ((pos = recvBuf.find('\n')) != std::string::npos)
        {
            std::string line = recvBuf.substr(0, pos);
            recvBuf.erase(0, pos + 1);
            processLine(line);
        }

        // Render
        window.clear(sf::Color(30, 30, 30));

        // Ground line
        sf::RectangleShape ground({ 800, 2 });
        ground.setPosition(0, 320);
        ground.setFillColor(sf::Color(100, 100, 100));
        window.draw(ground);

        window.draw(base1);
        window.draw(base2);

        // My units (green, move right)
        for (auto& u : myUnits)
        {
            sf::CircleShape c(12);
            c.setOrigin(12, 12);
            c.setPosition(u.x, 308);
            c.setFillColor(sf::Color(50, 200, 50));
            window.draw(c);
        }

        // Enemy units (red, move left)
        for (auto& u : enemyUnits)
        {
            sf::CircleShape c(12);
            c.setOrigin(12, 12);
            c.setPosition(u.x, 308);
            c.setFillColor(sf::Color(220, 50, 50));
            window.draw(c);
        }

        // My base HP bar
        float myHPFrac = std::max(0.f, (float)baseHP / 100.f);
        float enemyHPFrac = std::max(0.f, (float)enemyBaseHP / 100.f);

        hpBg.setSize({ 200, 18 });
        hpBg.setPosition(10, 570);
        window.draw(hpBg);
        hpBar.setSize({ 200.f * myHPFrac, 18 });
        hpBar.setPosition(10, 570);
        window.draw(hpBar);

        hpBg.setPosition(590, 570);
        window.draw(hpBg);
        hpBar.setSize({ 200.f * enemyHPFrac, 18 });
        hpBar.setFillColor(sf::Color(200, 50, 50));
        hpBar.setPosition(590, 570);
        window.draw(hpBar);
        hpBar.setFillColor(sf::Color::Green);  // reset

        // HUD text
        std::string hud =
            "Coins: " + std::to_string(coins) +
            "   My HP: " + std::to_string(baseHP) +
            "   Enemy HP: " + std::to_string(enemyBaseHP);
        infoText.setString(hud);
        window.draw(infoText);

        // Per-unit spawn slot bars (show cooldown progress)
        const int   slotCost[5] = { 5, 7, 10, 12, 15 };
        const char* slotLabel[5] = { "1", "2", "3", "4", "5" };
        const sf::Color READY(50, 200, 50);
        const sf::Color NOTREADY(120, 120, 120);
        const sf::Color NOCOIN(180, 60, 60);

        sf::RectangleShape slotBg({ 60, 40 });
        sf::RectangleShape slotFill({ 60, 40 });
        sf::Text slotText;
        slotText.setFont(font);
        slotText.setCharacterSize(13);

        for (int i = 0; i < 5; i++)
        {
            float sx = 10.f + i * 70.f;
            float sy = 38.f;

            float elapsed = keyClock[i].getElapsedTime().asSeconds();
            float fraction = std::min(elapsed / KEY_COOLDOWN[i], 1.f);
            bool  ready = fraction >= 1.f;
            bool  canAfford = coins >= slotCost[i];

            // Background
            slotBg.setPosition(sx, sy);
            slotBg.setFillColor(sf::Color(40, 40, 40));
            window.draw(slotBg);

            // Progress fill
            slotFill.setSize({ 60.f * fraction, 40.f });
            slotFill.setPosition(sx, sy);
            slotFill.setFillColor(ready ? (canAfford ? READY : NOCOIN) : NOTREADY);
            window.draw(slotFill);

            // Label: key number + cost
            slotText.setString("[" + std::string(slotLabel[i]) + "]\n$" + std::to_string(slotCost[i]));
            slotText.setFillColor(sf::Color::White);
            slotText.setPosition(sx + 4.f, sy + 4.f);
            window.draw(slotText);
        }

        if (!gameStatus.empty())
        {
            statusText.setString(gameStatus);
            window.draw(statusText);
        }

        window.display();
    }

    close(sock);
}