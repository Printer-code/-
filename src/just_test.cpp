#include "just_test.h"
#include <QApplication>
#include <QRandomGenerator>
#include <QDebug>
#include <QFont>
#include <QPushButton>
#include <QPen>
#include <QGraphicsColorizeEffect>
#include <QBrush>
#include <QDateTime>

// 固定窗口尺寸 480*800  3:5比例
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

    // 初始化背景
    bgItem = new QGraphicsPixmapItem(QPixmap(":/images/background/5.png").scaled(SCREEN_W, SCREEN_H, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    scene->addItem(bgItem);

    pauseMask = new QGraphicsRectItem(0, 0, SCREEN_W, SCREEN_H);
    pauseMask->setBrush(QColor(0, 0, 0, 180));
    pauseMask->setZValue(99);
    pauseMask->hide();
    scene->addItem(pauseMask);

    initAudio();
    initUI();

    // 角色奔跑/自行车雪碧图帧切割
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
    // ★ 修复点1：使用更标准的 qrc:/// 前缀，避免路径解析失败
    bgmPlayer = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    bgmPlayer->setAudioOutput(audioOutput);
    audioOutput->setVolume(0.4);
    isMuted = false;
    bgmPlayer->setSource(QUrl("qrc:///bgm/bgm1.mp3"));
    bgmPlayer->setLoops(QMediaPlayer::Infinite);

    jumpPlayer = new QMediaPlayer(this);
    jumpAudio = new QAudioOutput(this);
    jumpPlayer->setAudioOutput(jumpAudio);
    jumpAudio->setVolume(0.8);
    jumpPlayer->setSource(QUrl("qrc:///bgm/jumping.mp3"));

    slidePlayer = new QMediaPlayer(this);
    slideAudio = new QAudioOutput(this);
    slidePlayer->setAudioOutput(slideAudio);
    slideAudio->setVolume(1.0);
    slidePlayer->setSource(QUrl("qrc:///bgm/slipping.mp3"));

    swingPlayer = new QMediaPlayer(this);
    swingAudio = new QAudioOutput(this);
    swingPlayer->setAudioOutput(swingAudio);
    swingAudio->setVolume(0.8);
    swingPlayer->setSource(QUrl("qrc:///bgm/swing.wav"));

    hitPlayer = new QMediaPlayer(this);
    hitAudio = new QAudioOutput(this);
    hitPlayer->setAudioOutput(hitAudio);
    hitAudio->setVolume(1.0);
    hitPlayer->setSource(QUrl("qrc:///bgm/hit.wav"));

    coinPlayer = new QMediaPlayer(this);
    coinAudio = new QAudioOutput(this);
    coinPlayer->setAudioOutput(coinAudio);
    coinAudio->setVolume(0.4);
    coinPlayer->setSource(QUrl("qrc:///bgm/coin.wav"));

    eatPlayer = new QMediaPlayer(this);
    eatAudio = new QAudioOutput(this);
    eatPlayer->setAudioOutput(eatAudio);
    eatAudio->setVolume(1.0);
    eatPlayer->setSource(QUrl("qrc:///bgm/eat.wav"));

    bikePlayer = new QMediaPlayer(this);
    bikeAudio = new QAudioOutput(this);
    bikePlayer->setAudioOutput(bikeAudio);
    bikeAudio->setVolume(1.0);
    bikePlayer->setSource(QUrl("qrc:///bgm/bell.wav"));
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

    QPixmap titlePix = QPixmap(":/images/UI/title.png");
    titlePix = titlePix.scaled(360, 9999, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    titleItem = new QGraphicsPixmapItem(titlePix);
    titleItem->setPos(SCREEN_W / 2 - titleItem->boundingRect().width() / 2, 60);
    titleItem->setZValue(15);
    scene->addItem(titleItem);
    titleItem->hide();

    tutorialMask = new QGraphicsRectItem(0, 0, SCREEN_W, SCREEN_H);
    tutorialMask->setBrush(QColor(0, 0, 0, 220));
    tutorialMask->setZValue(30);
    scene->addItem(tutorialMask);
    tutorialMask->hide();

    QPixmap insPix = QPixmap(":/images/UI/instruction.png");
    insPix = insPix.scaled(SCREEN_W - 20, SCREEN_H - 100, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    tutorialImage = new QGraphicsPixmapItem(insPix);
    tutorialImage->setPos(SCREEN_W / 2 - tutorialImage->boundingRect().width() / 2, 80);
    tutorialImage->setZValue(31);
    scene->addItem(tutorialImage);
    tutorialImage->hide();

    QPushButton *closeBtn = new QPushButton();
    closeBtn->setFixedSize(40, 40);
    closeBtn->setStyleSheet(R"(
        QPushButton {
            border-image: url(:/images/UI/close.png);
            background-color: transparent;
        }
    )");
    closeTutorialWidget = scene->addWidget(closeBtn);
    closeTutorialWidget->setPos(SCREEN_W - 55, 15);
    closeTutorialWidget->setZValue(32);
    connect(closeBtn, SIGNAL(clicked()), this, SLOT(closeTutorial()));
    closeTutorialWidget->hide();

    // 维持上一版高度提升后的饱满尺寸
    QPushButton *tutorialBtn = new QPushButton("游戏教程");
    tutorialBtn->setFixedSize(180, 75);
    tutorialBtn->setStyleSheet(R"(
        QPushButton {
            border-image: url(:/images/UI/sides.png);
            font-size: 20px;
            font-weight: bold;
            color: white;
            background-color: transparent;
        }
    )");
    tutorialWidget = scene->addWidget(tutorialBtn);
    tutorialWidget->setPos(SCREEN_W / 2 - 90, SCREEN_H / 2 + 10);
    tutorialWidget->setZValue(20);
    connect(tutorialBtn, SIGNAL(clicked()), this, SLOT(onTutorialClicked()));

    QPushButton *startBtn = new QPushButton("开始游戏");
    startBtn->setFixedSize(180, 75);
    startBtn->setStyleSheet(R"(
        QPushButton {
            border-image: url(:/images/UI/main.png);
            font-size: 20px;
            font-weight: bold;
            color: white;
            background-color: transparent;
        }
    )");
    startWidget = scene->addWidget(startBtn);
    startWidget->setPos(SCREEN_W / 2 - 90, SCREEN_H / 2 + 95);
    startWidget->setZValue(20);
    connect(startBtn, SIGNAL(clicked()), this, SLOT(onStartClicked()));

    QPushButton *pauseBtn = new QPushButton();
    pauseBtn->setFixedSize(40, 40);
    pauseBtn->setStyleSheet(R"(
        QPushButton {
            border-image: url(:/images/UI/pause.png);
            background-color: transparent;
        }
    )");
    pauseWidget = scene->addWidget(pauseBtn);
    pauseWidget->setPos(SCREEN_W - 55, 15);
    pauseWidget->setZValue(25);
    connect(pauseBtn, SIGNAL(clicked()), this, SLOT(onPauseClicked()));
    pauseWidget->hide();

    QPushButton *resumeBtn = new QPushButton();
    resumeBtn->setFixedSize(40, 40);
    resumeBtn->setStyleSheet(R"(
        QPushButton {
            border-image: url(:/images/UI/play.png);
            background-color: transparent;
        }
    )");
    resumeWidget = scene->addWidget(resumeBtn);
    resumeWidget->setPos(SCREEN_W - 55, 15);
    resumeWidget->setZValue(25);
    connect(resumeBtn, SIGNAL(clicked()), this, SLOT(onResumeClicked()));
    resumeWidget->hide();

    QPushButton *pauseCloseBtn = new QPushButton();
    pauseCloseBtn->setFixedSize(40, 40);
    pauseCloseBtn->setStyleSheet(R"(
        QPushButton {
            border-image: url(:/images/UI/close.png);
            background-color: transparent;
        }
    )");
    pauseCloseWidget = scene->addWidget(pauseCloseBtn);
    pauseCloseWidget->setPos(SCREEN_W - 105, 15);
    pauseCloseWidget->setZValue(25);
    connect(pauseCloseBtn, SIGNAL(clicked()), this, SLOT(closeTutorial()));
    pauseCloseWidget->hide();

    gameOverText = new QGraphicsTextItem("游戏结束!");
    gameOverText->setFont(QFont("Microsoft YaHei", 40, QFont::Bold));
    gameOverText->setDefaultTextColor(Qt::red);
    gameOverText->setPos(SCREEN_W / 2 - 120, SCREEN_H / 2 - 150);
    gameOverText->setZValue(20);
    scene->addItem(gameOverText);

    QPushButton *restartBtn = new QPushButton("再来一局");
    restartBtn->setFixedSize(180, 55);
    restartBtn->setStyleSheet("background-color: #4CAF50; color: white; font-size: 22px; border-radius: 10px;");
    restartWidget = scene->addWidget(restartBtn);
    restartWidget->setPos(SCREEN_W / 2 - 90, SCREEN_H / 2 - 30);
    restartWidget->setZValue(20);
    connect(restartBtn, SIGNAL(clicked()), this, SLOT(onRestartClicked()));

    QPushButton *exitBtn = new QPushButton("退出游戏");
    exitBtn->setFixedSize(180, 55);
    exitBtn->setStyleSheet("background-color: #f44336; color: white; font-size: 22px; border-radius: 10px;");
    exitWidget = scene->addWidget(exitBtn);
    exitWidget->setPos(SCREEN_W / 2 - 90, SCREEN_H / 2 + 40);
    exitWidget->setZValue(20);
    connect(exitBtn, SIGNAL(clicked()), this, SLOT(onQuitClicked()));

    gameOverText->hide();
    restartWidget->hide();
    exitWidget->hide();
    tutorialWidget->hide();
}

void JustTestGame::clearEntities()
{
    for (int i = 0; i < obstacles.size(); i++)
    {
        scene->removeItem(obstacles[i]);
        delete obstacles[i];
    }
    obstacles.clear();

    for (int i = 0; i < coins.size(); i++)
    {
        scene->removeItem(coins[i]);
        delete coins[i];
    }
    coins.clear();

    for (int i = 0; i < clouds.size(); i++)
    {
        scene->removeItem(clouds[i]);
        delete clouds[i];
    }
    clouds.clear();
}

void JustTestGame::resetGame()
{
    clearEntities();

    score = 0;
    lives = 3;
    gameSpeed = 5.0;
    currentLane = 1;
    currentAction = RUNNING;
    hasBike = false;
    isInvincible = false;
    jumpVelocity = 0;
    frameCounter = 0;
    currentFrameIndex = 0;
    wantsToSlide = false;

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
        if (currentLane > 0)
        {
            currentLane--;
            // ★ 修复点2：使用 setPosition(0) 解决音效被吞的问题
            swingPlayer->setPosition(0);
            swingPlayer->play();
        }
        break;
    case Qt::Key_Right:
        if (currentLane < 2)
        {
            currentLane++;
            swingPlayer->setPosition(0);
            swingPlayer->play();
        }
        break;
    case Qt::Key_Up:
        if (currentAction == RUNNING || currentAction == SLIDING)
        {
            resetPlayerAction();
            currentAction = JUMPING;
            jumpVelocity = -11;
            playerItem->setScale(1.2);
            playerItem->setZValue(10);
            jumpPlayer->setPosition(0);
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
            slidePlayer->setPosition(0);
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

void JustTestGame::spawnEntities()
{
    static int lastObstacleType = -1;
    static qint64 lastBikeTime = 0;
    static qint64 lastFishTime = 0;
    static int consecutiveDoubles = 0;

    static bool lastWasCoinStreak = false;
    static bool lastWasItem = false;

    const qint64 BIKE_COOLDOWN = 30000;
    const qint64 FISH_COOLDOWN = 20000;

    QList<int> freeLanes;
    freeLanes << 0 << 1 << 2;

    int obstacleProb = 45 + (score / 100) * 2;
    if (obstacleProb > 75)
        obstacleProb = 75;

    int obsCount = 0;
    if (QRandomGenerator::global()->bounded(100) < obstacleProb)
    {
        int doubleObsProb = 10 + (score / 100) * 5;
        if (doubleObsProb > 70)
            doubleObsProb = 70;

        if (QRandomGenerator::global()->bounded(100) < doubleObsProb)
        {
            obsCount = 2;
        }
        else
        {
            obsCount = 1;
        }
    }

    if (obsCount == 2)
    {
        consecutiveDoubles++;
        if (consecutiveDoubles > 2)
        {
            obsCount = 1;
            consecutiveDoubles = 0;
        }
    }
    else
    {
        consecutiveDoubles = 0;
    }

    for (int c = 0; c < obsCount; c++)
    {
        if (freeLanes.isEmpty())
            break;
        int laneIdx = QRandomGenerator::global()->bounded(freeLanes.size());
        int lane = freeLanes.takeAt(laneIdx);

        QGraphicsPixmapItem *obs = new QGraphicsPixmapItem();
        int obsType;
        do
        {
            obsType = QRandomGenerator::global()->bounded(3);
        } while (obsType == lastObstacleType);
        lastObstacleType = obsType;

        if (obsType == 0)
            obs->setPixmap(QPixmap(":/images/obstacles/fir_tree_8.png"));
        else if (obsType == 1)
            obs->setPixmap(QPixmap(":/images/obstacles/jungle_tree_10.png"));
        else
            obs->setPixmap(QPixmap(":/images/obstacles/jungle_tree_1.png"));

        obs->setData(0, obsType == 0 ? JUMP_OVER : (obsType == 1 ? SLIDE_UNDER : DODGE_ONLY));
        obs->setPos(LANE_X[lane] - obs->boundingRect().width() / 2, -50);
        obs->setZValue(6);
        scene->addItem(obs);
        obstacles.append(obs);
    }

    bool spawnedItemThisTick = false;
    if (!lastWasItem && !freeLanes.isEmpty())
    {
        if (QRandomGenerator::global()->bounded(100) < 15)
        {
            int itemType = QRandomGenerator::global()->bounded(2);
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            bool canSpawn = false;

            if (itemType == 0 && (currentTime - lastFishTime >= FISH_COOLDOWN))
            {
                canSpawn = true;
                lastFishTime = currentTime;
            }
            else if (itemType == 1 && (currentTime - lastBikeTime >= BIKE_COOLDOWN))
            {
                canSpawn = true;
                lastBikeTime = currentTime;
            }

            if (canSpawn)
            {
                int laneIdx = QRandomGenerator::global()->bounded(freeLanes.size());
                int lane = freeLanes.takeAt(laneIdx);

                QGraphicsPixmapItem *item = new QGraphicsPixmapItem();
                if (itemType == 0)
                {
                    item->setPixmap(QPixmap(":/images/objects/fish.png").scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    item->setData(0, 888);
                }
                else
                {
                    item->setPixmap(QPixmap(":/images/objects/22.png").scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    item->setData(0, 999);
                }
                item->setPos(LANE_X[lane] - item->boundingRect().width() / 2, -50);
                item->setZValue(6);
                scene->addItem(item);
                obstacles.append(item);
                spawnedItemThisTick = true;
            }
        }
    }
    lastWasItem = spawnedItemThisTick;

    bool spawnedCoinThisTick = false;
    if (!lastWasCoinStreak && !freeLanes.isEmpty())
    {
        int coinProb = (obsCount == 0) ? 70 : 40;

        if (QRandomGenerator::global()->bounded(100) < coinProb)
        {
            int laneIdx = QRandomGenerator::global()->bounded(freeLanes.size());
            int lane = freeLanes.takeAt(laneIdx);

            int coinCount = QRandomGenerator::global()->bounded(3, 6);
            for (int i = 0; i < coinCount; i++)
            {
                QGraphicsPixmapItem *coin = new QGraphicsPixmapItem();
                coin->setPixmap(QPixmap(":/images/objects/7.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                coin->setPos(LANE_X[lane] - coin->boundingRect().width() / 2, -50 - i * 40);
                scene->addItem(coin);
                coins.append(coin);
            }
            spawnedCoinThisTick = true;
        }
    }
    lastWasCoinStreak = spawnedCoinThisTick;

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
                slidePlayer->setPosition(0);
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
    if (gameSpeed > 14.0)
    {
        gameSpeed = 14.0;
    }

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
            coinPlayer->setPosition(0);
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

            if (type == 999)
            {
                bikePlayer->setPosition(0);
                bikePlayer->play();
                hasBike = true;
                scene->removeItem(obs);
                delete obs;
                obstacles.removeAt(i);
                continue;
            }

            if (type == 888)
            {
                eatPlayer->setPosition(0);
                eatPlayer->play();
                if (lives < 3)
                {
                    lives++;
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
            hitPlayer->setPosition(0);
            hitPlayer->play();

            if (hasBike)
            {
                hasBike = false;
                isInvincible = true;
                QTimer::singleShot(1000, this, SLOT(endInvincible()));
                resetPlayerAction();
                playerItem->setPixmap(runFrames[currentFrameIndex]);
            }
            else
            {
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
        clearEntities();

        bgItem->setPixmap(QPixmap(":/images/UI/menu.png").scaled(SCREEN_W, SCREEN_H, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        titleItem->show();
        startWidget->show();
        tutorialWidget->show();
        playerItem->hide();
        gameOverText->hide();
        restartWidget->hide();
        exitWidget->hide();
        laneLine1->hide();
        laneLine2->hide();
        pauseMask->hide();
        scoreText->hide();

        tutorialMask->hide();
        tutorialImage->hide();
        closeTutorialWidget->hide();
        pauseWidget->hide();
        resumeWidget->hide();
        pauseCloseWidget->hide();

        for (auto heart : lifeHearts)
        {
            heart->hide();
        }
        break;

    case TUTORIAL:
        titleItem->hide();
        startWidget->hide();
        tutorialWidget->hide();
        playerItem->hide();
        gameOverText->hide();
        restartWidget->hide();
        exitWidget->hide();
        pauseMask->hide();
        scoreText->hide();
        pauseWidget->hide();
        resumeWidget->hide();
        pauseCloseWidget->hide();

        tutorialMask->show();
        tutorialImage->show();
        closeTutorialWidget->show();
        break;

    case PLAYING:
        bgItem->setPixmap(QPixmap(":/images/background/5.png").scaled(SCREEN_W, SCREEN_H, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        titleItem->hide();
        isInvincible = false;
        startWidget->hide();
        tutorialWidget->hide();
        gameOverText->hide();
        restartWidget->hide();
        exitWidget->hide();
        scoreText->show();
        playerItem->show();
        laneLine1->show();
        laneLine2->show();
        pauseMask->hide();

        tutorialMask->hide();
        tutorialImage->hide();
        closeTutorialWidget->hide();
        pauseWidget->show();
        resumeWidget->hide();
        pauseCloseWidget->hide();

        for (int i = 0; i < 3; i++)
        {
            if (i < lives)
                lifeHearts[i]->show();
            else
                lifeHearts[i]->hide();
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
        titleItem->hide();

        tutorialMask->hide();
        tutorialImage->hide();
        closeTutorialWidget->hide();
        startWidget->hide();
        tutorialWidget->hide();
        gameOverText->hide();
        restartWidget->hide();
        exitWidget->hide();
        pauseWidget->hide();
        resumeWidget->show();
        pauseCloseWidget->show();
        break;

    case GAMEOVER:
        timer->stop();
        spawnTimer->stop();
        bgmPlayer->stop();
        gameOverText->show();
        restartWidget->show();
        exitWidget->show();
        tutorialWidget->hide();
        pauseMask->hide();
        titleItem->hide();

        tutorialMask->hide();
        tutorialImage->hide();
        closeTutorialWidget->hide();
        pauseWidget->hide();
        resumeWidget->hide();
        pauseCloseWidget->hide();

        for (auto heart : lifeHearts)
        {
            heart->hide();
        }
        break;
    }
}

void JustTestGame::onPauseClicked()
{
    setGameState(PAUSED);
}

void JustTestGame::onResumeClicked()
{
    setGameState(PLAYING);
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

void JustTestGame::onQuitClicked()
{
    QApplication::quit();
}

void JustTestGame::onTutorialClicked()
{
    setGameState(TUTORIAL);
}

void JustTestGame::closeTutorial()
{
    setGameState(TITLE);
}

void JustTestGame::onToggleVolumeClicked()
{
    isMuted = !isMuted;
    audioOutput->setVolume(isMuted ? 0 : 0.5);
}

void JustTestGame::updatePerspectiveScale(QGraphicsPixmapItem *item)
{
}