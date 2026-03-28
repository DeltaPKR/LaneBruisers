#include <SFML/Graphics.hpp>
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <string>
#include <cmath>
#include <random>

#define SERVER_IP "127.0.0.1"
#define PORT 54000

struct Unit { float x; int hp; int type; };

sf::Texture generateBackground()                                           
{                                                                          
    sf::Image img;                                                         
    img.create(800, 600, sf::Color::Black);                                

    // Sky: horizontal colour bands for gradient look 
    struct Band { int y0, y1; uint8_t r, g, b; };                         
    Band bands[] = {                                                       
        {  0,  70, 12,  5, 30},   // deep purple                    
        { 70, 140, 22, 10, 55},   // dark violet                          
        {140, 200, 60, 20, 75},   // purple                               
        {200, 252,130, 50, 70},   // mauve-rose                           
        {252, 295,205, 95, 50},   // orange                          
        {295, 320,230,168, 55},   // gold horizon                         
    };                                                                     
    for (auto& b : bands)                                                  
        for (int y = b.y0; y < b.y1; y++)                                 
            for (int x = 0; x < 800; x++)                                 
                img.setPixel(x, y, { b.r, b.g, b.b });                     

    // Stars: fixed seed so the pattern is identical every run             
    std::mt19937 rng(42);                                                  
    std::uniform_int_distribution<int> distX(0, 799);                     
    std::uniform_int_distribution<int> distY(0, 185);                     
    std::uniform_int_distribution<int> distB(130, 255);                   
    for (int i = 0; i < 90; i++)                                          
    {                                                                      
        int b = distB(rng);                                                
        img.setPixel(distX(rng), distY(rng),                              
            { (uint8_t)b, (uint8_t)b, (uint8_t)b });               
    }                                                                      

    // Moon: solid pixel disc, top-right area                              
    const int mx = 668, my = 68, mr = 22;                                 
    for (int y = my - mr; y <= my + mr; y++)                              
        for (int x = mx - mr; x <= mx + mr; x++)                         
            if (x >= 0 && x < 800 && y >= 0 &&                           
                (x - mx) * (x - mx) + (y - my) * (y - my) <= mr * mr)                  
                img.setPixel(x, y, { 235, 225, 172 });                      

    // Hill silhouettes: 3 layers, each darker/closer than the last       
    // Using sin() with different frequencies gives organic pixel shapes   
    auto drawHills = [&](double freq1, double freq2,                       
        double phase2, int baseY,                        
        int amp1, int amp2,                              
        uint8_t r, uint8_t g, uint8_t bv)               
        {                                                                      
            for (int x = 0; x < 800; x++)                                     
            {                                                                  
                int top = baseY                                                
                    + (int)(amp1 * sin(x * freq1))                        
                    + (int)(amp2 * sin(x * freq2 + phase2));              
                for (int y = top; y < 320; y++)                               
                    img.setPixel(x, y, { r, g, bv });                           
            }                                                                  
        };                                                                     
    drawHills(0.013, 0.029, 1.1, 222, 32, 14, 18, 22, 42); // far       
    drawHills(0.017, 0.038, 0.4, 255, 24, 11, 28, 32, 55); // mid       
    drawHills(0.021, 0.047, 2.5, 279, 16, 9, 18, 38, 25); // near      

    // Ground fill                                                         
    for (int y = 320; y < 600; y++)                                       
        for (int x = 0; x < 800; x++)                                     
            img.setPixel(x, y, { 30, 22, 14 });                             

    // Grass strip along the top of the ground                            
    for (int y = 320; y < 327; y++)                                       
        for (int x = 0; x < 800; x++)                                     
            img.setPixel(x, y, { 34, 70, 22 });                             

    // Dirt lane: slightly lighter strip where units walk                  
    for (int y = 308; y < 320; y++)                                       
        for (int x = 42; x < 758; x++)                                    
            img.setPixel(x, y, { 52, 40, 25 });                             

    // Rock pattern on the lane (two offset rows of stones)         
    const sf::Color mortar = { 38, 28, 16 };                                
    for (int row = 0; row < 2; row++)                                     
    {                                                                      
        int baseY = 309 + row * 5;                                       
        int offset = row * 11;                                             
        for (int col = 0; col < 50; col++)                                
        {                                                                  
            int bx = 42 + col * 22 + offset;                              
            if (bx + 20 >= 758) break;                                    
            // horizontal mortar lines (top + bottom of stone)             
            for (int dx = 0; dx <= 20; dx++)                              
            {                                                              
                img.setPixel(bx + dx, baseY, mortar);                 
                img.setPixel(bx + dx, baseY + 4, mortar);                 
            }                                                              
            // vertical mortar lines (left + right)                       
            for (int dy = 0; dy < 4; dy++)                                
            {                                                              
                img.setPixel(bx, baseY + dy, mortar);                
                img.setPixel(bx + 20, baseY + dy, mortar);                
            }                                                              
        }                                                                  
    }                                                                      

    sf::Texture tex;                                                       
    tex.loadFromImage(img);                                                
    tex.setSmooth(false);  // keep pixel-art crispness          
    return tex;                                                            
}                                                                          

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

    sf::Texture bgTex = generateBackground();                           
    sf::Sprite  bgSprite(bgTex);                                          

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
    hintText.setString("Press ESC to quit");
    centerText(hintText, 800, 558);

    sf::RectangleShape menuOverlay({ 800, 600 });                          
    menuOverlay.setFillColor(sf::Color(0, 0, 0, 155));

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

        window.clear(sf::Color::Black);                                
        window.draw(bgSprite);                                            
        window.draw(menuOverlay);                                                                 
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

    sf::Texture towerTex;
    towerTex.setSmooth(false);
    if (!towerTex.loadFromFile("TowerSprite.png")) 
    {
        std::cerr << "Could not load TowerSprite.png\n";
        return 1;
    }

    sf::Texture unitTex[5];

    std::string files[5] =
    {
        "Unit1Sprite.png",
        "Unit2Sprite.png",
        "Unit3Sprite.png",
        "Unit4Sprite.png",
        "Unit5Sprite.png"
    };

    for (int i = 0; i < 5; i++)
    {
        if (!unitTex[i].loadFromFile(files[i]))
        {
            std::cerr << "Could not load Unit" << i << "Sprite.png\n";
            return 1;
        }

        unitTex[i].setSmooth(false);

    }

    auto tsz = towerTex.getSize();                                 
    const float TOWER_SCALE = 1.5f;

    sf::Sprite base1(towerTex);    
    base1.setScale(TOWER_SCALE, TOWER_SCALE);
    base1.setPosition(0.f, 270.f - (float)tsz.y);                 

    sf::Sprite base2(towerTex);                                   
    base2.setTextureRect({ (int)tsz.x, 0, -(int)tsz.x, (int)tsz.y });
    base2.setScale(TOWER_SCALE, TOWER_SCALE);
    base2.setPosition(740.f - (int)tsz.x, 270.f - (float)tsz.y);

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
                    Unit u; ss >> u.x >> u.hp >> u.type;
                    u1list.push_back(u);
                }
                ss >> tag >> count;   // "U2" <n>
                for (int i = 0; i < count; i++)
                {
                    Unit u; ss >> u.x >> u.hp >> u.type;
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
        window.clear(sf::Color::Black);
        window.draw(bgSprite); // draw background first

        window.draw(base1);
        window.draw(base2);

        // My units (green, move right)
        for (auto& u : myUnits)
        {
            sf::Sprite s(unitTex[u.type]);

            auto sz = unitTex[u.type].getSize();
            s.setOrigin(sz.x / 2.f, sz.y / 1.5f);
            s.setPosition(u.x, 308);
            s.setScale(2.f, 2.f);

            window.draw(s);
        }

        // Enemy units (red, move left)
        for (auto& u : enemyUnits)
        {
            sf::Sprite s(unitTex[u.type]);

            auto sz = unitTex[u.type].getSize();
            s.setOrigin(sz.x / 2.f, sz.y / 1.5f);
            s.setPosition(u.x, 308);
            s.setScale(-2.f, 2.f);   // flip horizontally

            window.draw(s);
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