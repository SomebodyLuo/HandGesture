#include "handgesturedialog.h"
#include "ui_handgesturedialog.h"
#include <QProgressDialog>

extern map<string, string> g_ConfigMap;
extern map<string, vector<string>> g_TargetMap;

HandGestureDialog::HandGestureDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HandGestureDialog)
{
    ui->setupUi(this);

    cam = NULL;
    frame = NULL;
    p_CurSnapImg = NULL;
    m_imgShowMode = ISM_RGB;

    isRun = 1;
    time_intervals = 100;
    timer = new QTimer(this);
    gesturecount = 0;
    status_switch = Nothing;
    pt = NULL;
    r = 0;
    afterSkin = NULL;
    connect(timer, SIGNAL(timeout()), this, SLOT(readFarme()));
    ui->pushButton_CloseCamera->setDisabled (true);
    ui->pushButton_ShowPause->setDisabled (true);
    ui->pushButton_SnapImage->setDisabled (true);
}

HandGestureDialog::~HandGestureDialog()
{
    delete ui;
}

void HandGestureDialog::StartRecongizeHand (IplImage *img) {
    // Create a string that contains the exact cascade name
    // Contains the trained classifer for detecting hand
    string cascade_name_str = g_ConfigMap["ROOTDIR"] + g_ConfigMap["HANDFILE"];
    const char *cascade_name= cascade_name_str.c_str();
    // Create memory for calculations
    static CvMemStorage* storage = 0;
    // Create a new Haar classifier
    static CvHaarClassifierCascade* cascade = 0;
    // Sets the scale with which the rectangle is drawn with
    int scale = 1;
    // Create two points to represent the hand locations
    CvPoint pt1, pt2;
    // Looping variable
    int i;
    // Load the HaarClassifierCascade
    cascade = (CvHaarClassifierCascade*)cvLoad( cascade_name, 0, 0, 0 );
    // Check whether the cascade has loaded successfully. Else report and error and quit
    if(!cascade) {
        cerr << "ERROR: Could not load classifier cascade" << endl;
        return;
    }

    storage = cvCreateMemStorage(0);
    cvClearMemStorage( storage );

    // Find whether the cascade is loaded, to find the hands. If yes, then:
    if(cascade) {
        // There can be more than one hand in an image. So create a growable sequence of hands.
        // Detect the objects and store them in the sequence
        CvSeq* hands = cvHaarDetectObjects( img, cascade, storage,
                                            1.1, 2, CV_HAAR_DO_CANNY_PRUNING,
                                            cvSize(40, 40) );

        // Loop the number of hands found.
        for( i = 0; i < (hands ? hands->total : 0); i++ ) {
            // Create a new rectangle for drawing the hand
            CvRect* r = (CvRect*)cvGetSeqElem( hands, i );

            // Find the dimensions of the hand,and scale it if necessary
            pt1.x = r->x*scale;
            pt2.x = (r->x+r->width)*scale;
            pt1.y = r->y*scale;
            pt2.y = (r->y+r->height)*scale;

            // Draw the rectangle in the input image
            cvRectangle( img, pt1, pt2, CV_RGB(230,20,232), 3, 8, 0 );
        }
    }
}

void HandGestureDialog::readFarme() {
    frame = cvQueryFrame(cam);
    if(frame->origin == IPL_ORIGIN_BL) {
        cvFlip(frame, frame, IPL_ORIGIN_TL);
    }

    emit grabImageFinished(frame);

    IplImage* pTmp = cvCloneImage(frame);
    cvtImageShowMode(frame, pTmp);

    if(status_switch == Recongnise) {
        // Flips the frame into mirror image

        // Call the function to detect and draw the hand positions
        StartRecongizeHand(pTmp);
    }

    {
        QImage image;
        IplImage2QImage(pTmp, image);

        image = image.scaled(320,240);
        ui->label_CameraShow->setPixmap(QPixmap::fromImage(image));
    }

    {
        QImage image;
        IplImage* grayImg = cvCreateImage(cvGetSize(pTmp), IPL_DEPTH_8U, 1);
        cvCvtColor(pTmp, grayImg, CV_BGR2GRAY);
        IplImage2QImage(grayImg, image);
        image = image.scaled(320, 240);
        ui->label_CameraShow_Gray->setPixmap(QPixmap::fromImage(image));
//        p_MyLabel->setPixmap(QPixmap::fromImage(image));
    }

    cvReleaseImage(&pTmp);
}

void HandGestureDialog::on_pushButton_OpenCamera_clicked() {
    cam = cvCreateCameraCapture(0);
    timer->start(time_intervals);
    frame = cvQueryFrame(cam);

    ui->pushButton_OpenCamera->setDisabled (true);
    ui->pushButton_CloseCamera->setEnabled (true);
    ui->pushButton_ShowPause->setEnabled (true);
    ui->pushButton_SnapImage->setEnabled (true);
    afterSkin = cvCreateImage (cvSize(frame->width,frame->height),IPL_DEPTH_8U,1);
}

void HandGestureDialog::on_pushButton_SnapImage_clicked() {
    if(frame != NULL) {
        p_CurSnapImg = cvCloneImage(frame);
        cvtImageShowMode(frame, p_CurSnapImg);
        QImage image;
        IplImage2QImage(p_CurSnapImg, image);

        image = image.scaled(220, 200);
        ui->label_ShowSnap->setPixmap(QPixmap::fromImage(image));
    }
}

void HandGestureDialog::on_pushButton_CloseCamera_clicked()
{
    timer->stop();
    cvReleaseCapture(&cam);
    cam = NULL;
    ui->pushButton_OpenCamera->setEnabled (true);
    ui->pushButton_CloseCamera->setDisabled (true);
    ui->pushButton_ShowPause->setDisabled (true);
    ui->pushButton_SnapImage->setDisabled (true);
}

void HandGestureDialog::on_pushButton_ShowPause_clicked() {
    if(isRun == 1) {
        timer->stop();
        QTextCodec *codec = QTextCodec::codecForName("utf-8");
        QString str = codec->toUnicode("重新开始");
        ui->pushButton_ShowPause->setText(str);
    } else {
        timer->start(time_intervals);
        QTextCodec *codec = QTextCodec::codecForName("utf-8");
        QString str = codec->toUnicode("暂停");
        ui->pushButton_ShowPause->setText(str);
    }
    isRun *= (-1);
}

void HandGestureDialog::on_pushButton_StartTrain_clicked() {
    QProgressDialog* process = new QProgressDialog(this);
    process->setWindowTitle (tr("Traning Model"));
    process->setLabelText(tr("Processing..."));
    process->setModal(true);
    process->show ();
    gesture.setMainUIPointer (this);
    gesture.Train(process);
    delete process;
    QMessageBox::about (this,tr("Finished !"),tr("Basic Hand Gesture Recognition !"));
}

void HandGestureDialog::on_pushButton_StartRecongnise_clicked() {
    if(cam==NULL) {
        QMessageBox::warning (this,tr("Warning"),tr("Please Check Camera !"));
        return;
    }

    if(status_switch == Recongnise) {
        QTextCodec *codec = QTextCodec::codecForName("utf-8");
        QString str = codec->toUnicode("开始手势识别");
        ui->pushButton_StartRecongnise->setText(str);
        status_switch = Nothing;
    } else {
        QTextCodec *codec = QTextCodec::codecForName("utf-8");
        QString str = codec->toUnicode("暂停手势识别");
        ui->pushButton_StartRecongnise->setText(str);
        status_switch = Recongnise;
    }
}

void HandGestureDialog::on_comboBox_ShowDelay_activated(int index)
{
    /*from ZERO*/
    time_intervals = index*100;
    if(timer == NULL)
        return ;
    if(timer->isActive ())
    {
        timer->stop ();
        timer->start (time_intervals);
    }
}

void HandGestureDialog::on_comboBox_ImageMode_activated(int index) {
    /*some kind image to convert*/
    if(index == 0) {
        m_imgShowMode = ISM_RGB;
    }
    if(index == 1) {
        m_imgShowMode = ISM_HSV;
    }
    if(index == 2) {
        m_imgShowMode = ISM_YCBCR;
    }
    if(index == 3) {
        m_imgShowMode == ISM_YUV;
    }
}

void HandGestureDialog::on_pushButton_StartGestureReg_clicked() {

}

bool HandGestureDialog::IplImage2QImage(const IplImage *src, QImage &dest) {
    switch(src->depth) {
    case IPL_DEPTH_8U:
        if(src->nChannels == 1) {
            dest = QImage((const uchar*)src->imageData, src->width, src->height, QImage::Format_Grayscale8);
        } else if(src->nChannels == 3) {
            uchar newData[src->width*src->height*4*sizeof(uchar)];
            const uchar* imgData = (const uchar*) src->imageData;
            int k=0;
            for(int row=0;row<src->height;++row) {
                for(int col=0;col<src->width;++col) {
                    newData[k++] = imgData[0];
                    newData[k++] = imgData[1];
                    newData[k++] = imgData[2];
                    newData[k++] = 0;
                    imgData += 3;
                }
                imgData += (src->widthStep - 3 * src->width);
            }

            dest = QImage(newData, src->width, src->height, QImage::Format_RGB32);
        } else if(src->nChannels == 4) {
            dest = QImage((const uchar*)src->imageData, src->width, src->height, QImage::Format_ARGB32);
        }
        return true;
    }
    return false;
}

bool HandGestureDialog::QImage2IplImage(const QImage &src, IplImage &dest) {
    return false;
}

bool HandGestureDialog::CvMat2QImage(const cv::Mat& mat, QImage& qImg) {
    // 8-bits unsigned, NO. OF CHANNELS = 1
    if(mat.type() == CV_8UC1) {
        qImg = QImage(mat.cols, mat.rows, QImage::Format_Indexed8);
        // Set the color table (used to translate colour indexes to qRgb values)
        qImg.setColorCount(256);
        for(int i = 0; i < 256; i++) {
            qImg.setColor(i, qRgb(i, i, i));
        }
        // Copy input Mat
        uchar *pSrc = mat.data;
        for(int row = 0; row < mat.rows; row ++) {
            uchar *pDest = qImg.scanLine(row);
            memcpy(pDest, pSrc, mat.cols);
            pSrc += mat.step;
        }
        return true;
    }
    // 8-bits unsigned, NO. OF CHANNELS = 3
    if(mat.type() == CV_8UC3) {
        // Copy input Mat
        const uchar *pSrc = (const uchar*)mat.data;
        // Create QImage with same dimensions as input Mat
        qImg = QImage(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        qImg = qImg.rgbSwapped();
        return true;
    }

    if(mat.type() == CV_8UC4) {
        // Copy input Mat
        const uchar *pSrc = (const uchar*)mat.data;
        // Create QImage with same dimensions as input Mat
        qImg = QImage(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        qImg = qImg.copy();
        return true;
    }
    qDebug() << "ERROR: Mat could not be converted to QImage.";
    return false;
}

bool HandGestureDialog::QImage2CvMat(const QImage& qImg,cv::Mat& mat) {

}

bool HandGestureDialog::cvtImageShowMode(const IplImage *src, IplImage *dest) {
    switch(m_imgShowMode) {
    case ISM_RGB:
        //cvCvtColor(src, dest, CV_BGR2RGB);
        break;
    case ISM_HSV:
        cvCvtColor(src, dest, CV_BGR2HSV);
        break;
    case ISM_YCBCR:
        cvCvtColor(src, dest, CV_BGR2YCrCb);
        break;
    case ISM_YUV:
        cvCvtColor(src, dest, CV_BGR2YUV);
        break;
    case ISM_BINARY:
        break;
    default:
        cout << "No Such ISM_Mode !" << endl;
    }
}
