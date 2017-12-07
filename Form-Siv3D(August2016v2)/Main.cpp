#define NO_S3D_USING
#include<opencv2/opencv.hpp>
# include <Siv3D.hpp>
#include"../MatImageConvert/MatImageConvert.hpp"

//画像matから輪郭を抽出、内側の輪郭だけを返す
std::vector<std::vector<cv::Point>> GetInnerContours(const cv::Mat& mat)
{
        cv::Mat forContour;

        //グレスケ&二値化
        cv::cvtColor(mat, forContour, CV_RGBA2GRAY);
        cv::threshold(forContour, forContour, 200, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

        //ネガポジ反転　輪郭は白に対して取るため
        forContour = ~forContour;

        //輪郭を抽出
        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::Vec4i> hierarchy;

        cv::findContours(forContour, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE);

        std::vector<std::vector<cv::Point>> result;

        //hierarchyの情報から、親の存在する（＝内側の）輪郭だけを取る
        for (int i = 0; i < contours.size(); ++i)
        {
                if (hierarchy[i][3] != -1) result.push_back(contours[i]);
        }

        return result;
}

//オリジナル画像と別にキャンバス画像を管理し、輪郭の書き込みなどを行うクラス
class ContourViewer
{
private:
        cv::Mat m_origMat;       

        cv::Mat m_canvasMat;
        s3d::Image m_canvasImage;

        s3d::DynamicTexture m_texture;

public:
        ContourViewer(){}

        ContourViewer(const cv::Mat& mat)
        {
                m_origMat = mat;

                //canvasImageとcanvasMatはデータを共有する
                m_canvasImage = s3d::Image(mat.cols, mat.rows);

                m_canvasMat = cvsiv::GetMatLinkedToImage(m_canvasImage);

                //オリジナル画像をキャンバスへコピー
                m_origMat.copyTo(m_canvasMat);
                
                m_texture.fill(m_canvasImage);
        }

        void drawContours(const std::vector<std::vector<cv::Point>>& contours, const cv::Scalar& color = cv::Scalar(255, 0, 0, 255), int thickness = 2 )
        {
                //輪郭を書き込む前の画像をコピー
                m_origMat.copyTo(m_canvasMat);

                //輪郭を書き込む
                cv::drawContours(m_canvasMat, contours, -1, color, thickness);

                m_texture.fill(m_canvasImage);
        }

        const s3d::DynamicTexture& texture() &
        {
                return m_texture;
        }

};

void Main()
{
        const s3d::Font font(30);

        s3d::Window::SetStyle(s3d::WindowStyle::Sizeable);

        s3d::Window::Resize(800, 600);

        s3d::Image image(L"form.jpg");

        if (!image)
        {
                LOG(L"Image Load Failed");
                return;
        }

        cv::Mat mat = cvsiv::GetMatLinkedToImage(image);

        //内側の輪郭だけを取得
        auto contours = GetInnerContours(mat);

        //輪郭を元画像に書き込んで表示するためのクラス
        ContourViewer conview(mat);

        conview.drawContours(contours);

        //GUIの用意
        s3d::GUI gui(s3d::GUIStyle::Default);
        gui.setTitle(L"ゴミ取り");
        gui.addln(L"txt1", s3d::GUIText::Create(L"枠として扱う領域の下限面積を調整してください"));
        gui.addln(L"txtAreaValue", s3d::GUIText::Create(L"1"));
        gui.addln(L"sldAreaLower", s3d::GUISlider::Create(1, 40000, 1, 300, true));


        while (s3d::System::Update())
        {

                if (gui.slider(L"sldAreaLower").hasChanged)
                {
                        //GUIの更新
                        const double lowerLimit = gui.slider(L"sldAreaLower").value;

                        gui.text(L"txtAreaValue").text = s3d::Format(lowerLimit);

                        //面積の下限を条件に、描画する輪郭だけを抽出する
                        std::vector<std::vector<cv::Point>> drawContours;

                        for (auto& contour : contours)
                        {
                                if (cv::contourArea(contour, false) < lowerLimit) continue;
                                drawContours.push_back(contour);
                        
                        }

                        //書き込み
                        conview.drawContours(drawContours);
                }

                //GUIは常に右上に表示
                gui.setPos(s3d::Window::Size().x - 100, 0);

                //画面拡縮に合わせて画像を表示
                const double scale = 1.0*s3d::Window::Size().y / conview.texture().height;
                conview.texture().scale(scale).draw();


        }
}
