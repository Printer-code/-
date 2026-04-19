#include "just_test.h"
#include <QApplication>
#include <QRandomGenerator>
#include <QDebug>
#include <QFont>
#include <QPushButton>
#include <QPen>
#include <QGraphicsColorizeEffect>
#include <QBrush>

const int SCREEN_W = 480;
const int SCREEN_H = 800;
const int LANE_X[] = {100, 240, 380};
const int PLAYER_BASE_Y = 600;

JustTestGame::JustTestGame(QWidget *parent) : QGraphicsView(parent)
{
    setFixedSize(SCREEN_W, SCREEN_H);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    scene = new QGraphicsScene(0, 0, SCREEN_W, SCREEN_H, this);
    setScene(scene);

    bgItem = new QGraphicsPixmapItem(QPixmap(":/images/background/5.png").scaled(SCREEN_W, SCREEN_H, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    scene->addItem(bgItem);

    pauseMask = new QGraphicsRectItem(0, 0, SCREEN_W, SCREEN_H);
    pauseMask->setBrush(QColor(0, 0, 0, 180));
    pauseMask->setZValue(99);
    pauseMask->hide();
    scene->addItem(pauseMask);

    initAudio();
    initUI();

    QPixmap fullRunSheet(":/images/figure/running.png");
    QPixmap fullBikeSheet(":/images/figure/riding.png");

    int frameWidth_run = fullRunSheet.width() / 4;
    int frameHeight_run = fullRunSheet.height();
    for (int i = 0; i < 4; i++)
    {
        runFrames.append(fullRunSheet.copy(i * frameWidth_run, 0, frameWidth_run, frameHeight_run));
    }

    int frameWidth_bike = fullBikeSheet.width() / 4;
    int frameHeight_bike = fullBikeSheet.height();
    for (int i = 0; i < 4; i++)
    {
        bikeFrames.append(fullBikeSheet.copy(i * frameWidth_bike, 0, frameWidth_bike, frameHeight_bike));
    }

    currentFrameIndex = 0;
    frameCounter = 0;
    jumpVelocity = 0;
    wantsToSlide = false;

    playerItem = new QGraphicsPixmapItem(runFrames[0]);
    playerItem->setTransformOriginPoint(frameWidth_run / 2, frameHeight_run / 2);
    playerItem->setZValue(5);
    scene->addItem(playerItem);
    playerItem->hide();

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(gameLoop()));

    spawnTimer = new QTimer(this);
    connect(spawnTimer, SIGNAL(timeout()), this, SLOT(spawnEntities()));

    slideTimer = new QTimer(this);
    slideTimer->setSingleShot(true);
    connect(slideTimer, SIGNAL(timeout()), this, SLOT(resetPlayerAction()));

    setGameState(TITLE);
}

JustTestGame::~JustTestGame() {}

void JustTestGame::initAudio()
{
    // BGM
    bgmPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    bgmPlayer->setAudioOutput(audioOutput);
    audioOutput->setVolume(0.3);
    isMuted = false;
    bgmPlayer->setSource(QUrl("qrc:/bgm/bgm1.mp3"));
    bgmPlayer->setLoops(QMediaPlayer::Infinite); // BGM无限循环

    // 跳跃音效
    jumpPlayer = new QMediaPlayer(this);
    jumpAudio = new QAudioOutput(this);
    jumpPlayer->setAudioOutput(jumpAudio);
    jumpAudio->setVolume(0.6);
    jumpPlayer->setSource(QUrl("qrc:/bgm/jumping.mp3"));

    // 下滑音效
    slidePlayer = new QMediaPlayer(this);
    slideAudio = new QAudioOutput(this);
    slidePlayer->setAudioOutput(slideAudio);
    slideAudio->setVolume(1.2);
    slidePlayer->setSource(QUrl("qrc:/bgm/slipping.mp3"));

    // 左右移动音效
    swingPlayer = new QMediaPlayer(this);
    swingAudio = new QAudioOutput(this);
    swingPlayer->setAudioOutput(swingAudio);
    swingAudio->setVolume(0.7);
    swingPlayer->setSource(QUrl("qrc:/bgm/swing.wav"));

    // 受击音效
    hitPlayer = new QMediaPlayer(this);
    hitAudio = new QAudioOutput(this);
    hitPlayer->setAudioOutput(hitAudio);
    hitAudio->setVolume(1.0);
    hitPlayer->setSource(QUrl("qrc:/bgm/hit.wav"));

    // 金币音效
    coinPlayer = new QMediaPlayer(this);
    coinAudio = new QAudioOutput(this);
    coinPlayer->setAudioOutput(coinAudio);
    coinAudio->setVolume(0.1);
    coinPlayer->setSource(QUrl("qrc:/bgm/coin.wav"));

    // 吃鱼音效
    eatPlayer = new QMediaPlayer(this);
    eatAudio = new QAudioOutput(this);
    eatPlayer->setAudioOutput(eatAudio);
    eatAudio->setVolume(0.9);
    eatPlayer->setSource(QUrl("qrc:/bgm/eat.wav"));

    // 【新增】自行车音效
    bikePlayer = new QMediaPlayer(this);
    bikeAudio = new QAudioOutput(this);
    bikePlayer->setAudioOutput(bikeAudio);
    bikeAudio->setVolume(0.9);
    bikePlayer->setSource(QUrl("qrc:/bgm/bell.wav"));
}

void JustTestGame::initUI()
{
    QPen linePen(QColor(76, 145, 76));
    linePen.setWidth(5);

    qreal lineTopY = SCREEN_H / 3;
    laneLine1 = scene->addLine(170, lineTopY, 170, SCREEN_H, linePen);
    laneLine2 = scene->addLine(310, lineTopY, 310, SCREEN_H, linePen);
    laneLine1->setZValue(5);
    laneLine2->setZValue(5);

    scoreText = new QGraphicsTextItem("Score: 0");
    scoreText->setFont(QFont("Arial", 20, QFont::Bold));
    scoreText->setDefaultTextColor(Qt::yellow);
    scoreText->setPos(10, 10);
    scoreText->setZValue(10);
    scene->addItem(scoreText);
    scoreText->hide();

    // 初始化三格血条
    for (int i = 0; i < 3; i++)
    {
        QGraphicsPixmapItem *heart = new QGraphicsPixmapItem(
            QPixmap(":/images/figure/life.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        heart->setPos(10 + i * 35, 45);
        heart->setZValue(10);
        scene->addItem(heart);
        lifeHearts.append(heart);
        heart->hide();
    }

    QPushButton *startBtn = new QPushButton("开始游戏");
    startBtn->setFixedSize(200, 60);
    startBtn->setStyleSheet("background-color: rgba(255, 255, 250, 200); font-size: 24px; font-weight: bold; border-radius: 15px;");
    startWidget = scene->addWidget(startBtn);
    startWidget->setPos(SCREEN_W / 2 - 100, SCREEN_H / 2 + 50);
    startWidget->setZValue(20);
    connect(startBtn, SIGNAL(clicked()), this, SLOT(onStartClicked()));

    gameOverText = new QGraphicsTextItem("游戏结束!");
    gameOverText->setFont(QFont("Microsoft YaHei", 40, QFont::Bold));
    gameOverText->setDefaultTextColor(Qt::red);
    gameOverText->setPos(SCREEN_W / 2 - 120, SCREEN_H / 2 - 150);
    gameOverText->setZValue(20);
    scene->addItem(gameOverText);

    QPushButton *restartBtn = new QPushButton("再来一局");
    restartBtn->setFixedSize(160, 50);
    restartBtn->setStyleSheet("background-color: #4CAF50; color: white; font-size: 20px; border-radius: 10px;");
    restartWidget = scene->addWidget(restartBtn);
    restartWidget->setPos(SCREEN_W / 2 - 80, SCREEN_H / 2 - 30);
    restartWidget->setZValue(20);
    connect(restartBtn, SIGNAL(clicked()), this, SLOT(onRestartClicked()));

    QPushButton *exitBtn = new QPushButton("退出游戏");
    exitBtn->setFixedSize(160, 50);
    exitBtn->setStyleSheet("background-color: #f44336; color: white; font-size: 20px; border-radius: 10px;");
    exitWidget = scene->addWidget(exitBtn);
    exitWidget->setPos(SCREEN_W / 2 - 80, SCREEN_H / 2 + 40);
    exitWidget->setZValue(20);
    connect(exitBtn, SIGNAL(clicked()), this, SLOT(onQuitClicked()));

    gameOverText->hide();
    restartWidget->hide();
    exitWidget->hide();
}

void JustTestGame::resetGame()
{
    int i;
    for (i = 0; i < obstacles.size(); i++)
    {
        scene->removeItem(obstacles[i]);
        delete obstacles[i];
    }
    for (i = 0; i < coins.size(); i++)
    {
        scene->removeItem(coins[i]);
        delete coins[i];
    }
    for (i = 0; i < clouds.size(); i++)
    {
        scene->removeItem(clouds[i]);
        delete clouds[i];
    }
    obstacles.clear();
    coins.clear();
    clouds.clear();

    score = 0;
    lives = 3;
    gameSpeed = 5.0;
    currentLane = MID_LANE;
    currentAction = RUNNING;
    hasBike = false;
    isInvincible = false;
    jumpVelocity = 0;
    frameCounter = 0;
    currentFrameIndex = 0;
    wantsToSlide = false;

    // 显示所有爱心
    for (auto heart : lifeHearts)
    {
        heart->show();
    }

    playerItem->setPos(LANE_X[currentLane] - playerItem->boundingRect().width() / 2, PLAYER_BASE_Y);
    playerItem->setTransform(QTransform());
    playerItem->setScale(1.0);
    playerItem->setZValue(5);
    playerItem->setPixmap(runFrames[0]);
    playerItem->setGraphicsEffect(nullptr);
}

void JustTestGame::keyPressEvent(QKeyEvent *event)
{
    if (currentState == TITLE)
    {
        if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
        {
            resetGame();
            setGameState(PLAYING);
        }
        return;
    }

    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Escape)
    {
        if (currentState == PLAYING)
            setGameState(PAUSED);
        else if (currentState == PAUSED)
            setGameState(PLAYING);
        return;
    }

    if (currentState != PLAYING)
        return;

    switch (event->key())
    {
    case Qt::Key_Left:
        if (currentLane > LEFT_LANE)
        {
            currentLane--;
            swingPlayer->stop();
            swingPlayer->play();
        }
        break;
    case Qt::Key_Right:
        if (currentLane < RIGHT_LANE)
        {
            currentLane++;
            swingPlayer->stop();
            swingPlayer->play();
        }
        break;
    case Qt::Key_Up:
        // 【修改】跑步和下滑状态都能直接跳
        if (currentAction == RUNNING || currentAction == SLIDING)
        {
            // 先重置下滑状态（恢复人物大小和位置）
            resetPlayerAction();
            // 直接进入跳跃状态
            currentAction = JUMPING;
            jumpVelocity = -11;
            playerItem->setScale(1.2);
            playerItem->setZValue(10);
            jumpPlayer->stop();
            jumpPlayer->play();
            QTimer::singleShot(600, this, SLOT(resetPlayerAction()));
        }
        break;
    case Qt::Key_Down:
        if (currentAction == JUMPING)
        {
            jumpVelocity = 15.0;
            wantsToSlide = true;
        }
        else if (currentAction == RUNNING)
        {
            resetPlayerAction();
            currentAction = SLIDING;
            playerItem->setTransform(QTransform::fromScale(1.0, 0.4));
            playerItem->setZValue(0);
            slideTimer->start(800);
            slidePlayer->stop();
            slidePlayer->play();
        }
        break;
    default:
        break;
    }
}

void JustTestGame::resetPlayerAction()
{
    currentAction = RUNNING;
    playerItem->setTransform(QTransform());
    playerItem->setScale(1.0);
    playerItem->setY(PLAYER_BASE_Y);
    jumpVelocity = 0;
    playerItem->setZValue(5);
    slideTimer->stop();
    wantsToSlide = false;
}

void JustTestGame::endInvincible()
{
    isInvincible = false;
}

void JustTestGame::clearEffect()
{
    playerItem->setGraphicsEffect(nullptr);
}

// 生成实体：鱼道具5%概率，单车10%概率
void JustTestGame::spawnEntities()
{
    int randomType = QRandomGenerator::global()->bounded(100);
    int lane;

    static int lastObstacleType = -1;
    static bool lastIsTripleCoin = false;
    static int lastObstacleLane = -1;
    static qint64 lastBikeTime = 0;
    static qint64 lastFishTime = 0;
    static int lastObsCount = 1;
    const qint64 BIKE_COOLDOWN = 30000;
    const qint64 FISH_COOLDOWN = 20000;

    int obstacleProb = 65 + (score / 80) * 3;
    if (obstacleProb > 83)
        obstacleProb = 83;
    int bikeProb = 10;
    int fishProb = 5;

    if (randomType < obstacleProb)
    {
        int obsCount;
        if (lastObsCount == 2)
        {
            obsCount = 1;
        }
        else
        {
            obsCount = QRandomGenerator::global()->bounded(1, 3);
        }
        lastObsCount = obsCount;

        QList<int> usedLanes;

        for (int c = 0; c < obsCount; c++)
        {
            QGraphicsPixmapItem *obs = new QGraphicsPixmapItem();
            int obsType;

            do
            {
                obsType = QRandomGenerator::global()->bounded(3);
            } while (obsType == lastObstacleType);
            lastObstacleType = obsType;

            do
            {
                lane = QRandomGenerator::global()->bounded(3);
            } while (usedLanes.contains(lane));
            usedLanes.append(lane);
            lastObstacleLane = lane;
            lastIsTripleCoin = false;

            if (obsType == 0)
                obs->setPixmap(QPixmap(":/images/obstacles/fir_tree_8.png"));
            else if (obsType == 1)
                obs->setPixmap(QPixmap(":/images/obstacles/jungle_tree_10.png"));
            else
                obs->setPixmap(QPixmap(":/images/obstacles/jungle_tree_1.png"));

            obs->setData(0, obsType == 0 ? JUMP_OVER : (obsType == 1 ? SLIDE_UNDER : DODGE_ONLY));

            qreal obsY = -50;
            if (c > 0)
            {
                obsY -= QRandomGenerator::global()->bounded(70, 90);
            }
            obs->setPos(LANE_X[lane] - obs->boundingRect().width() / 2, obsY);
            obs->setZValue(6);
            scene->addItem(obs);
            obstacles.append(obs);
        }
    }
    else if (randomType < (obstacleProb + fishProb))
    {
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (currentTime - lastFishTime < FISH_COOLDOWN)
        {
            spawnEntities();
            return;
        }
        lastFishTime = currentTime;
        lastIsTripleCoin = false;
        lastObstacleType = -1;
        lane = QRandomGenerator::global()->bounded(3);
        QGraphicsPixmapItem *fish = new QGraphicsPixmapItem();
        fish->setPixmap(QPixmap(":/images/objects/fish.png").scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        fish->setData(0, 888);
        fish->setPos(LANE_X[lane] - fish->boundingRect().width() / 2, -50);
        fish->setZValue(6);
        scene->addItem(fish);
        obstacles.append(fish);
    }
    else if (randomType < (obstacleProb + fishProb + bikeProb))
    {
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (currentTime - lastBikeTime < BIKE_COOLDOWN)
        {
            spawnEntities();
            return;
        }
        lastBikeTime = currentTime;
        lastIsTripleCoin = false;
        lastObstacleType = -1;
        lane = QRandomGenerator::global()->bounded(3);
        QGraphicsPixmapItem *bike = new QGraphicsPixmapItem();
        bike->setPixmap(QPixmap(":/images/objects/22.png").scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        bike->setData(0, 999);
        bike->setPos(LANE_X[lane] - bike->boundingRect().width() / 2, -50);
        bike->setZValue(6);
        scene->addItem(bike);
        obstacles.append(bike);
    }
    else
    {
        if (lastIsTripleCoin)
        {
            spawnEntities();
            return;
        }
        lastIsTripleCoin = true;
        lastObstacleType = -1;
        lane = QRandomGenerator::global()->bounded(3);

        int coinCount = QRandomGenerator::global()->bounded(1, 8);
        for (int i = 0; i < coinCount; i++)
        {
            QGraphicsPixmapItem *coin = new QGraphicsPixmapItem();
            coin->setPixmap(QPixmap(":/images/objects/7.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            coin->setPos(LANE_X[lane] - coin->boundingRect().width() / 2, -50 - i * 40);
            scene->addItem(coin);
            coins.append(coin);
        }
    }

    if (QRandomGenerator::global()->bounded(100) < 50)
    {
        QGraphicsPixmapItem *cloud = new QGraphicsPixmapItem();
        cloud->setPixmap(QPixmap(":/images/background/cloud1.png"));
        cloud->setPos(QRandomGenerator::global()->bounded(SCREEN_W), -100);
        cloud->setZValue(-1);
        scene->addItem(cloud);
        clouds.append(cloud);
    }
}

void JustTestGame::gameLoop()
{
    int i;
    bool hit;

    frameCounter++;
    if (frameCounter >= 8)
    {
        frameCounter = 0;
        currentFrameIndex = (currentFrameIndex + 1) % 4;
        if (hasBike)
            playerItem->setPixmap(bikeFrames[currentFrameIndex]);
        else
            playerItem->setPixmap(runFrames[currentFrameIndex]);
    }

    qreal playerW = playerItem->boundingRect().width();
    qreal playerH = playerItem->boundingRect().height();
    qreal targetX = LANE_X[currentLane] - playerW / 2;
    playerItem->setX(playerItem->x() + (targetX - playerItem->x()) * 0.3);

    if (currentAction == JUMPING)
    {
        playerItem->setY(playerItem->y() + jumpVelocity);
        jumpVelocity += 0.6;

        if (playerItem->y() >= PLAYER_BASE_Y)
        {
            playerItem->setY(PLAYER_BASE_Y);

            if (wantsToSlide)
            {
                wantsToSlide = false;
                currentAction = SLIDING;
                playerItem->setTransform(QTransform::fromScale(1.0, 0.4));
                playerItem->setZValue(0);
                slideTimer->start(800);
                slidePlayer->stop();
                slidePlayer->play();
            }
            else
            {
                currentAction = RUNNING;
                playerItem->setScale(1.0);
                playerItem->setZValue(5);
            }
            jumpVelocity = 0;
        }
    }

    gameSpeed = 5.5 + (score / 100) * 0.6;

    for (i = clouds.size() - 1; i >= 0; i--)
    {
        QGraphicsPixmapItem *cloud = clouds[i];
        cloud->moveBy(0, gameSpeed * 0.3);
        if (cloud->y() > SCREEN_H)
        {
            scene->removeItem(cloud);
            delete cloud;
            clouds.removeAt(i);
        }
    }

    for (i = coins.size() - 1; i >= 0; i--)
    {
        QGraphicsPixmapItem *coin = coins[i];
        coin->moveBy(0, gameSpeed);
        if (coin->collidesWithItem(playerItem))
        {
            score += 10;
            scoreText->setPlainText("Score: " + QString::number(score));
            coinPlayer->stop();
            coinPlayer->play();
            scene->removeItem(coin);
            delete coin;
            coins.removeAt(i);
        }
        else if (coin->y() > SCREEN_H)
        {
            scene->removeItem(coin);
            delete coin;
            coins.removeAt(i);
        }
    }

    for (i = obstacles.size() - 1; i >= 0; i--)
    {
        QGraphicsPixmapItem *obs = obstacles[i];
        obs->moveBy(0, gameSpeed);
        hit = false;

        if (isInvincible)
        {
            if (obs->y() > SCREEN_H)
            {
                scene->removeItem(obs);
                delete obs;
                obstacles.removeAt(i);
            }
            continue;
        }

        if (obs->collidesWithItem(playerItem))
        {
            int type = obs->data(0).toInt();

            if (type == 999) // 单车
            {
                // 【新增】播放自行车音效
                bikePlayer->stop();
                bikePlayer->play();

                hasBike = true;
                scene->removeItem(obs);
                delete obs;
                obstacles.removeAt(i);
                continue;
            }

            if (type == 888) // 鱼：回血不加分
            {
                eatPlayer->stop();
                eatPlayer->play();

                // 回血，不超过3格上限
                if (lives < 3)
                {
                    lives++;
                    // 显示对应的爱心（从右往左恢复）
                    lifeHearts[lives - 1]->show();
                }

                scene->removeItem(obs);
                delete obs;
                obstacles.removeAt(i);
                continue;
            }

            qreal obsH = obs->boundingRect().height();
            qreal obsY = obs->y();

            qreal fixedPlayerDepth = PLAYER_BASE_Y + playerH / 2;
            qreal frontAreaTop = obsY + obsH * 0.7;
            qreal backAreaBottom = obsY + obsH + 10;
            bool isInObstacleDepth = (fixedPlayerDepth >= frontAreaTop && fixedPlayerDepth <= backAreaBottom);
            bool isPlayerOnGround = (playerItem->y() >= PLAYER_BASE_Y - 5);

            if (type == JUMP_OVER)
            {
                if (isInObstacleDepth && isPlayerOnGround && currentAction != JUMPING)
                {
                    hit = true;
                }
            }
            else if (type == DODGE_ONLY)
            {
                if (isInObstacleDepth)
                {
                    hit = true;
                }
            }
            else if (type == SLIDE_UNDER)
            {
                if (isInObstacleDepth)
                {
                    qreal obsW = obs->boundingRect().width();
                    qreal obsCenterX = obs->x() + obsW / 2;
                    qreal playerCenterX = playerItem->x() + playerW / 2;
                    bool inCenterLane = (qAbs(playerCenterX - obsCenterX) < obsW * 0.35);

                    if (inCenterLane)
                    {
                        if (currentAction != SLIDING)
                        {
                            hit = true;
                        }
                    }
                    else
                    {
                        if (isPlayerOnGround && currentAction != JUMPING)
                        {
                            hit = true;
                        }
                    }
                }
            }
        }

        if (hit)
        {
            // 播放受击音效
            hitPlayer->stop();
            hitPlayer->play();

            if (hasBike)
            {
                // 单车状态：只消耗单车，绝对不扣血
                hasBike = false;
                isInvincible = true;
                QTimer::singleShot(1000, this, SLOT(endInvincible()));
                resetPlayerAction();
                playerItem->setPixmap(runFrames[currentFrameIndex]);
            }
            else
            {
                // 无单车状态：扣血
                lives--;
                if (lives >= 0 && lives < lifeHearts.size())
                {
                    lifeHearts[lives]->hide();
                }
                isInvincible = true;
                QTimer::singleShot(1000, this, SLOT(endInvincible()));

                QGraphicsColorizeEffect *effect = new QGraphicsColorizeEffect(this);
                effect->setColor(Qt::red);
                effect->setStrength(1.0);
                playerItem->setGraphicsEffect(effect);
                QTimer::singleShot(200, this, SLOT(clearEffect()));

                if (lives <= 0)
                {
                    setGameState(GAMEOVER);
                    return;
                }
            }
        }

        if (obs->y() > SCREEN_H)
        {
            scene->removeItem(obs);
            delete obs;
            obstacles.removeAt(i);
        }
    }
}

void JustTestGame::setGameState(GameState state)
{
    currentState = state;
    switch (state)
    {
    case TITLE:
        startWidget->show();
        playerItem->hide();
        gameOverText->hide();
        restartWidget->hide();
        exitWidget->hide();
        laneLine1->hide();
        laneLine2->hide();
        pauseMask->hide();
        scoreText->hide();
        for (auto heart : lifeHearts)
        {
            heart->hide();
        }
        break;
    case PLAYING:
        isInvincible = false;
        startWidget->hide();
        gameOverText->hide();
        restartWidget->hide();
        exitWidget->hide();
        scoreText->show();
        playerItem->show();
        laneLine1->show();
        laneLine2->show();
        pauseMask->hide();
        for (auto heart : lifeHearts)
        {
            heart->show();
        }
        timer->start(1000 / 60);
        spawnTimer->start(800);
        bgmPlayer->play();
        break;
    case PAUSED:
        timer->stop();
        spawnTimer->stop();
        bgmPlayer->pause();
        pauseMask->show();
        break;
    case GAMEOVER:
        timer->stop();
        spawnTimer->stop();
        bgmPlayer->stop();
        gameOverText->show();
        restartWidget->show();
        exitWidget->show();
        pauseMask->hide();
        for (auto heart : lifeHearts)
        {
            heart->hide();
        }
        break;
    }
}

void JustTestGame::onRestartClicked()
{
    gameOverText->hide();
    restartWidget->hide();
    exitWidget->hide();
    resetGame();
    setGameState(PLAYING);
}

void JustTestGame::onStartClicked()
{
    if (currentState == TITLE)
    {
        resetGame();
        setGameState(PLAYING);
    }
}

void JustTestGame::onPauseClicked() {}
void JustTestGame::onResumeClicked() {}

void JustTestGame::onQuitClicked()
{
    QApplication::quit();
}

void JustTestGame::onToggleVolumeClicked()
{
    isMuted = !isMuted;
    audioOutput->setVolume(isMuted ? 0 : 0.5);
}

void JustTestGame::updatePerspectiveScale(QGraphicsPixmapItem *item)
{
}