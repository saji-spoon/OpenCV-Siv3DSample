#define NO_S3D_USING
#include <Siv3D.hpp>
#include<opencv2/opencv.hpp>
#include "../MatImageConvert/MatImageConvert.hpp"
#include<algorithm>

//画像matから輪郭を抽出、内側の輪郭のうち一番面積が大きいものを、ApproxyPolyDPで直線近似して返す
std::vector<cv::Point> GetCorrectionContour(const cv::Mat& mat)
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

        std::vector<std::vector<cv::Point>> inner;

        //hierarchyの情報から、親の存在する（＝内側の）輪郭だけを取る
        for (int i = 0; i < contours.size(); ++i)
        {
                if (hierarchy[i][3] != -1) inner.push_back(contours[i]);
        }

        auto maxIt =  std::max_element(inner.begin(), inner.end(), [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) { return cv::contourArea(a, false) < cv::contourArea(b, false); });
        
        if (maxIt == inner.end()) return  std::vector<cv::Point>();

        auto contour = *maxIt;

        std::vector<cv::Point> poly;

        double arcRate = 0.005;

        cv::approxPolyDP(cv::Mat(contour), poly, arcRate * cv::arcLength(contour, true), true);

        return poly;
}

void Main()
{
        s3d::Window::SetStyle(s3d::WindowStyle::Sizeable);

        s3d::Image baseFormImage(L"Form.jpg");

        s3d::Image writtenFormImage(L"writtenForm.jpg");

        if(!baseFormImage || !writtenFormImage)
        {
                LOG_ERROR(L"Image Load Failed.");
                return;
        }

        //白紙のアンケート用紙イメージ
        cv::Mat baseMat = cvsiv::GetMatLinkedToImage(baseFormImage);
        //書き込まれたアンケート用紙イメージ
        cv::Mat writtenMat = cvsiv::GetMatLinkedToImage(writtenFormImage);

        //それぞれについて補正枠を検出する
        //歪んでいる書き込み済み画像の補正枠を、元画像の補正枠に合わせる形で変換するので、書き込み済み=src, 元画像=dst
        auto dstContour = GetCorrectionContour(baseMat);
        auto srcContour = GetCorrectionContour(writtenMat);
       
        //本来はここで四角形（＝点が4つ）が取れているかチェックしたい（省略）

        //白紙のアンケート用紙イメージに補正枠を書き込む
        cv::drawContours(baseMat, std::vector<std::vector<cv::Point>>{ dstContour }, -1, cv::Scalar(0, 0, 255, 255), 2);

        //writtenMatの補正枠を見せるための別画像
        //（元画像は後で補正後の状態を見せるため）
        auto writtenMatPreview = writtenMat.clone();

        //検出された補正枠を書き込み
        cv::drawContours(writtenMatPreview, std::vector<std::vector<cv::Point>>{ srcContour }, -1, cv::Scalar(0, 0, 255, 255), 2);

        //ここから書き込み済み画像の補正
        //getPerspectiveTransform用にsrc/dstContourを変換
        cv::Point2f src[4];
        for (int i = 0; i < 4; ++i)
        {
                src[i] = srcContour[i];
        }
        cv::Point2f dst[4];
        for (int i = 0; i < 4; ++i)
        {
                dst[i] = dstContour[i];
        }

        //元画像をwarpPerspectiveで補正するための行列を取得
        cv::Mat trans = cv::getPerspectiveTransform(src, dst);

        cv::Mat corrected;

        //透視変換で補正
        cv::warpPerspective(writtenMat, corrected, trans, cv::Size(baseMat.cols, baseMat.rows),1,0,cv::Scalar(255,255,255,255));

        //GUIの用意
        s3d::GUI gui(s3d::GUIStyle::Default);
        gui.setTitle(L"スキュー補正");
        gui.addln(L"txtInfo", s3d::GUIText::Create(L"白紙画像＋補正枠"));
        gui.addln(L"btnNext", s3d::GUIButton::Create(L"次へ"));
        gui.addln(L"txtPadding", s3d::GUIText::Create(L"　　　　　　　"));

        //初期状態では白紙画像＋検出された補正枠を表示
        s3d::Texture texture(cvsiv::MatToImage(baseMat));

        int mode = 0;
       
	while (s3d::System::Update())
	{
                switch (mode)
                {
                case 0:
                        if(gui.button(L"btnNext").pushed)
                        {
                                //補正前の書き込み済み画像＋検出された補正枠の表示
                                texture = s3d::Texture(cvsiv::MatToImage(writtenMatPreview));
                                gui.text(L"txtInfo").text = L"補正前画像＋検出した補正枠";
                                mode = 1;
                        }
                        break;
                case 1:
                        if (gui.button(L"btnNext").pushed)
                        {
                                //補正後の書き込み済み画像を表示
                                texture = s3d::Texture(cvsiv::MatToImage(corrected));
                                gui.text(L"txtInfo").text = L"補正後";
                                gui.button(L"btnNext").enabled = false;
                                mode = 2;
                        }
                        break;
                case 2:
                        break;
                default:
                        break;
                }

                //GUIは常に右上に表示
                gui.setPos(s3d::Window::Size().x - 100, 0);

                const double scale = 1.0*s3d::Window::Size().y / texture.height;
                texture.scale(scale).draw();


	}
}
