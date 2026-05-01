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
#include <QKeyEvent>

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

    QGraphicsTextItem *tutorialText = new QGraphicsTextItem("按 ↑ ↓ ← → 进行躲避", tutorialMask);
    tutorialText->setFont(QFont("Microsoft YaHei", 22, QFont::Bold));
    tutorialText->setDefaultTextColor(Qt::white);
    tutorialText->setPos(SCREEN_W / 2 - tutorialText->boundingRect().width() / 2, 35);

    QPixmap insPix = QPixmap(":/images/UI/instruction.png");
    insPix = insPix.scaled(SCREEN_W - 20, SCREEN_H - 100, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    tutorialImage = new QGraphicsPixmapItem(insPix);
    tutorialImage->setPos(SCREEN_W / 2 - tutorialImage->boundingRect().width() / 2, 90);
    tutorialImage->setZValue(31);
    scene->addItem(tutorialImage);
    tutorialImage->hide();

    QPushButton *closeBtn = new QPushButton();
    closeBtn->setFixedSize(40, 40);
    closeBtn->setStyleSheet(R"(
        QPushButton {
            border-image: url(:/images/UI/close.png);
            background-color: transparent;
            border: none;
            padding: 0px;
            margin: 0px;
        }
    )");
    closeTutorialWidget = scene->addWidget(closeBtn);
    closeTutorialWidget->setPos(SCREEN_W - 55, 15);
    closeTutorialWidget->setZValue(32);
    connect(closeBtn, SIGNAL(clicked()), this, SLOT(closeTutorial()));
    closeTutorialWidget->hide();

    // ====================== 按钮图片原始宽高比 3:2，宽度180时高度应为120 ======================
    int btnWidth = 180;
    int btnHeight = 120; // 高度 = 宽度 * 2/3 = 120

    // 游戏教程按钮（图片 sides.png 比例 3:2）
    QPushButton *tutorialBtn = new QPushButton("游戏教程");
    tutorialBtn->setFixedSize(btnWidth, btnHeight);
    tutorialBtn->setStyleSheet(R"(
        QPushButton {
            border-image: url(:/images/UI/sides.png);
            border: none;
            padding: 0px;
            margin: 0px;
            outline: none;
            font-size: 20px;
            font-weight: bold;
            color: white;
            background-color: transparent;
            min-height: 0px;
            max-height: 16777215px;
        }
        QPushButton:pressed {
            padding-top: 2px;
            padding-left: 2px;
        }
    )");
    tutorialWidget = scene->addWidget(tutorialBtn);
    tutorialWidget->setGeometry(QRectF(0, 0, btnWidth, btnHeight));
    tutorialWidget->setPos(SCREEN_W / 2 - btnWidth / 2, SCREEN_H / 2 + 10);
    tutorialWidget->setZValue(20);
    connect(tutorialBtn, SIGNAL(clicked()), this, SLOT(onTutorialClicked()));

    // 开始游戏按钮（图片 main.png 比例 3:2）
    QPushButton *startBtn = new QPushButton("开始游戏");
    startBtn->setFixedSize(btnWidth, btnHeight);
    startBtn->setStyleSheet(R"(
        QPushButton {
            border-image: url(:/images/UI/main.png);
            border: none;
            padding: 0px;
            margin: 0px;
            outline: none;
            font-size: 20px;
            font-weight: bold;
            color: white;
            background-color: transparent;
            min-height: 0px;
            max-height: 16777215px;
        }
        QPushButton:pressed {
            padding-top: 2px;
            padding-left: 2px;
        }
    )");
    startWidget = scene->addWidget(startBtn);
    startWidget->setGeometry(QRectF(0, 0, btnWidth, btnHeight));
    // 由于按钮高度增加到120，下方按钮位置需下移更多，避免重叠
    startWidget->setPos(SCREEN_W / 2 - btnWidth / 2, SCREEN_H / 2 + 130);
    startWidget->setZValue(20);
    connect(startBtn, SIGNAL(clicked()), this, SLOT(onStartClicked()));

    // 暂停 / 恢复等小按钮保持不变
    QPushButton *pauseBtn = new QPushButton();
    pauseBtn->setFixedSize(40, 40);
    pauseBtn->setStyleSheet(R"(
        QPushButton {
            border-image: url(:/images/UI/pause.png);
            background-color: transparent;
            border: none;
            padding: 0px;
            margin: 0px;
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
            border: none;
            padding: 0px;
            margin: 0px;
        }
    )");
    resumeWidget = scene->addWidget(resumeBtn);
    resumeWidget->setPos(SCREEN_W - 55, 15);
    resumeWidget->setZValue(100);
    connect(resumeBtn, SIGNAL(clicked()), this, SLOT(onResumeClicked()));
    resumeWidget->hide();

    QPushButton *pauseCloseBtn = new QPushButton();
    pauseCloseBtn->setFixedSize(40, 40);
    pauseCloseBtn->setStyleSheet(R"(
        QPushButton {
            border-image: url(:/images/UI/close.png);
            background-color: transparent;
            border: none;
            padding: 0px;
            margin: 0px;
        }
    )");
    pauseCloseWidget = scene->addWidget(pauseCloseBtn);
    pauseCloseWidget->setPos(SCREEN_W - 105, 15);
    pauseCloseWidget->setZValue(100);
    connect(pauseCloseBtn, SIGNAL(clicked()), this, SLOT(closeTutorial()));
    pauseCloseWidget->hide();

    gameOverText = new QGraphicsTextItem("游戏结束!");
    gameOverText->setFont(QFont("Microsoft YaHei", 40, QFont::Bold));
    gameOverText->setDefaultTextColor(Qt::red);
    gameOverText->setPos(SCREEN_W / 2 - 120, SCREEN_H / 2 - 150);
    gameOverText->setZValue(20);
    scene->addItem(gameOverText);

    // 再来一局按钮（图片 main.png，同样使用 3:2 高度）
    QPushButton *restartBtn = new QPushButton("再来一局");
    restartBtn->setFixedSize(btnWidth, btnHeight);
    restartBtn->setStyleSheet(R"(
        QPushButton {
            border-image: url(:/images/UI/main.png);
            border: none;
            padding: 0px;
            margin: 0px;
            outline: none;
            font-size: 20px;
            font-weight: bold;
            color: white;
            background-color: transparent;
            min-height: 0px;
            max-height: 16777215px;
        }
        QPushButton:pressed {
            padding-top: 2px;
            padding-left: 2px;
        }
    )");
    restartWidget = scene->addWidget(restartBtn);
    restartWidget->setGeometry(QRectF(0, 0, btnWidth, btnHeight));
    restartWidget->setPos(SCREEN_W / 2 - btnWidth / 2, SCREEN_H / 2 - 30);
    restartWidget->setZValue(20);
    connect(restartBtn, SIGNAL(clicked()), this, SLOT(onRestartClicked()));

    // 返回主菜单按钮（图片 sides.png，同样使用 3:2 高度）
    QPushButton *exitBtn = new QPushButton("返回主菜单");
    exitBtn->setFixedSize(btnWidth, btnHeight);
    exitBtn->setStyleSheet(R"(
        QPushButton {
            border-image: url(:/images/UI/sides.png);
            border: none;
            padding: 0px;
            margin: 0px;
            outline: none;
            font-size: 20px;
            font-weight: bold;
            color: white;
            background-color: transparent;
            min-height: 0px;
            max-height: 16777215px;
        }
        QPushButton:pressed {
            padding-top: 2px;
            padding-left: 2px;
        }
    )");
    exitWidget = scene->addWidget(exitBtn);
    exitWidget->setGeometry(QRectF(0, 0, btnWidth, btnHeight));
    exitWidget->setPos(SCREEN_W / 2 - btnWidth / 2, SCREEN_H / 2 + 50);
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
    scoreText->setPlainText("Score: 0");

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
    if (event->isAutoRepeat())
        return;

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
            jumpVelocity = -12.0;
            playerItem->setScale(1.2);
            playerItem->setZValue(10);
            jumpPlayer->setPosition(0);
            jumpPlayer->play();
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

    // coinCooldown：大于0时表示处于金币生成冷却期
    static int coinCooldown = 0;
    static bool lastWasItem = false;

    const qint64 BIKE_COOLDOWN = 30000;
    const qint64 FISH_COOLDOWN = 20000;

    QList<int> freeLanes;
    freeLanes << 0 << 1 << 2;
//障碍生成部分
    // 随着分数增加，障碍生成概率逐渐提升，最高可达75%，防止难度过高
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
        else
        {
            // 强制将两个障碍物分别放置在最左侧和最右侧赛道，确保中间留空且贴图不重叠
            freeLanes.clear();
            freeLanes << 0 << 2;
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
        // 记录上一个生成的障碍类型，避免连续生成相同类型
        lastObstacleType = obsType;

        if (obsType == 0)
            obs->setPixmap(QPixmap(":/images/obstacles/fir_tree_8.png"));
        else if (obsType == 1)
            obs->setPixmap(QPixmap(":/images/obstacles/jungle_tree_10.png"));
        else
            obs->setPixmap(QPixmap(":/images/obstacles/jungle_tree_1.png"));

        obs->setData(0, obsType == 0 ? JUMP_OVER : (obsType == 1 ? SLIDE_UNDER : DODGE_ONLY));
        // 给每个障碍设置100-220的随机Y偏移，避免同一高度同时出现
        int randomYOffset = QRandomGenerator::global()->bounded(100, 220);
        obs->setPos(LANE_X[lane] - obs->boundingRect().width() / 2, -50 - randomYOffset);
        obs->setZValue(6);
        scene->addItem(obs);
        obstacles.append(obs);
    }

//道具生成部分
    // 在没有连续生成道具的情况下，且当前有空闲赛道时，有15%概率生成道具
    bool spawnedItemThisTick = false;
    if (!lastWasItem && !freeLanes.isEmpty())
    {
        if (QRandomGenerator::global()->bounded(100) < 15)
        {
            int itemType = QRandomGenerator::global()->bounded(2);
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            bool canSpawn = false;
            // 鱼类道具和自行车道具分别有独立的冷却时间，确保它们不会在短时间内频繁出现
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
                // 道具也同步做偏移，和障碍错开
                int itemYOffset = QRandomGenerator::global()->bounded(80, 180);
                item->setPos(LANE_X[lane] - item->boundingRect().width() / 2, -50 - itemYOffset);
                item->setZValue(6);
                scene->addItem(item);
                obstacles.append(item);
                spawnedItemThisTick = true;
            }
        }
    }
    lastWasItem = spawnedItemThisTick;
//金币生成部分
    if (coinCooldown > 0)
    {
        coinCooldown--;
    }

    if (coinCooldown <= 0 && !freeLanes.isEmpty())
    {
        int coinProb = (obsCount == 0) ? 25 : 10;

        if (QRandomGenerator::global()->bounded(100) < coinProb)
        {
            int laneIdx = QRandomGenerator::global()->bounded(freeLanes.size());
            int lane = freeLanes.takeAt(laneIdx);

            int coinCount = QRandomGenerator::global()->bounded(3, 6);
            // 金币组整体做随机偏移，避免每次都在同一跑道生成
            int coinGroupOffset = QRandomGenerator::global()->bounded(50, 120);
            for (int i = 0; i < coinCount; i++)
            {
                QGraphicsPixmapItem *coin = new QGraphicsPixmapItem();
                coin->setPixmap(QPixmap(":/images/objects/7.png").scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                coin->setPos(LANE_X[lane] - coin->boundingRect().width() / 2, -50 - coinGroupOffset - i * 40);
                scene->addItem(coin);
                coins.append(coin);
            }
            // 金币生成后进入冷却期，避免短时间内大量金币出现，冷却期长度根据当前难度动态调整
            coinCooldown = 2;
        }
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
    // 计算目标X位置，确保角色中心对齐赛道中心，并添加平滑过渡效果
    qreal targetX = LANE_X[currentLane] - playerW / 2;
    // 平滑过渡：每帧移动距离为当前与目标X的差值的30%，避免瞬移
    playerItem->setX(playerItem->x() + (targetX - playerItem->x()) * 0.3);

    if (currentAction == JUMPING)
    {
        // 角色跳跃时，Y轴位置根据jumpVelocity变化，形成抛物线效果
        playerItem->setY(playerItem->y() + jumpVelocity);
        jumpVelocity += 0.6;//此处设置了重力加速度

        if (playerItem->y() >= PLAYER_BASE_Y)
        {
            playerItem->setY(PLAYER_BASE_Y);

            if (wantsToSlide)
            {
                wantsToSlide = false;
                // 允许落地后直接进入滑铲状态，缩小角色高度并降低Z值，持续800ms后恢复
                //实现了跳跃和滑铲的无缝衔接
                currentAction = SLIDING;
                playerItem->setTransform(QTransform::fromScale(1.0, 0.4));
                // 跳跃结束后直接进入滑铲状态，降低Z值确保角色图层在障碍物下方
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

    // 分数达到1100后不再继续提升gameSpeed上限难度
    int speedScore = score > 1100 ? 1100 : score;
    gameSpeed = 5.5 + (speedScore / 100) * 0.6;
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
            // 计算障碍物的有效碰撞区域
            qreal obsH = obs->boundingRect().height();
            qreal obsY = obs->y();
            // 角色的碰撞深度固定在身体中部往下10个像素
            qreal fixedPlayerDepth = PLAYER_BASE_Y + playerH / 2;
            qreal frontAreaTop = obsY + obsH * 0.7;
            qreal backAreaBottom = obsY + obsH + 10;
            bool isInObstacleDepth = (fixedPlayerDepth >= frontAreaTop && fixedPlayerDepth <= backAreaBottom);
            bool isPlayerOnGround = (playerItem->y() >= PLAYER_BASE_Y - 5);

            if (type == JUMP_OVER)
            {
                // 跳跃类障碍物：只有当角色在碰撞区域内且未正确跳过时才算碰撞
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
                    // 计算角色中心X与障碍物中心X的距离，判断是否在障碍物中间范围内
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
                // 角色失去自行车后短暂无敌，避免连续碰撞导致瞬间失去多条生命
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
    setGameState(TITLE);
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