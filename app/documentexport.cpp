#include "documentexport.h"

#include <zip.h>

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryFile>

#ifdef Q_OS_WIN
#include <iowin32.h>
#endif

#include <algorithm>
#include <cmath>

namespace {
struct ZipArchive {
    zipFile handle;
    ~ZipArchive() { if (handle) zipClose(handle, nullptr); }

    bool add(const QByteArray &name, const QByteArray &data)
    {
        if (zipOpenNewFileInZip(handle, name.constData(), nullptr, nullptr, 0, nullptr, 0,
                                nullptr, Z_DEFLATED, Z_DEFAULT_COMPRESSION) != ZIP_OK)
            return false;
        const int written = zipWriteInFileInZip(handle, data.constData(), data.size());
        const int closed = zipCloseFileInZip(handle);
        return written == ZIP_OK && closed == ZIP_OK;
    }

    bool close()
    {
        const int status = zipClose(handle, nullptr);
        handle = nullptr;
        return status == ZIP_OK;
    }
};

QSizeF printSize(const AnnotationPage &page)
{
    return page.size * std::min<qreal>(1, annotationPrintableRect().width() / page.size.width());
}
}

QVector<AnnotationPage> createAnnotationPages(const QVector<Segment> &segments, qreal width,
                                             AnnotationOptions options, QString *error)
{
    if (segments.isEmpty() || !std::isfinite(width) || width <= 24) {
        if (error) *error = QStringLiteral("没有可导出的注音稿，或预览区域太窄。");
        return {};
    }
    const auto lines = layoutAnnotation(segments, width, options);
    const int perPage = annotationLinesPerPage(width, options);
    QVector<AnnotationPage> pages;
    for (int first = 0; first < lines.size(); first += perPage) {
        const QImage image = renderAnnotationPage(lines, first, perPage, width, options);
        QByteArray png;
        QBuffer buffer(&png);
        if (image.isNull() || !buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
            if (error) *error = QStringLiteral("无法绘制注音稿页面，未完成导出。");
            return {};
        }
        pages.append({png, QSizeF(image.size()) / image.devicePixelRatio()});
    }
    return pages;
}

bool exportAnnotatedDocx(const QString &path, const QVector<AnnotationPage> &pages, QString *error)
{
    if (pages.isEmpty()) {
        if (error) *error = QStringLiteral("没有可导出的注音稿页面。");
        return false;
    }
    QTemporaryFile temporary(QFileInfo(path).absolutePath() + "/.sing-japanese-XXXXXX.docx");
    if (!temporary.open()) {
        if (error) *error = QStringLiteral("无法创建 DOCX 临时文件：%1").arg(temporary.errorString());
        return false;
    }
    temporary.close();
#ifdef Q_OS_WIN
    zlib_filefunc64_def fileFunctions{};
    fill_win32_filefunc64W(&fileFunctions);
    ZipArchive archive{zipOpen2_64(temporary.fileName().toStdWString().c_str(),
                                  APPEND_STATUS_CREATE, nullptr, &fileFunctions)};
#else
    ZipArchive archive{zipOpen64(QFile::encodeName(temporary.fileName()).constData(), APPEND_STATUS_CREATE)};
#endif
    if (!archive.handle) {
        if (error) *error = QStringLiteral("无法创建 DOCX 文档。");
        return false;
    }
    const QByteArray contentTypes =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Default Extension=\"png\" ContentType=\"image/png\"/>"
        "<Override PartName=\"/word/document.xml\" "
        "ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "</Types>";
    const QByteArray rootRelationships =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" "
        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" "
        "Target=\"word/document.xml\"/></Relationships>";
    QString document = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
        "xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" "
        "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
        "xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\"><w:body>");
    QString relationships = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">");
    bool success = archive.add("[Content_Types].xml", contentTypes)
                   && archive.add("_rels/.rels", rootRelationships);
    for (int index = 0; success && index < pages.size(); ++index) {
        const auto &page = pages.at(index);
        const QSizeF size = printSize(page);
        const qint64 cx = qRound64(size.width() * 9525);
        const qint64 cy = qRound64(size.height() * 9525);
        const QString id = QString::number(index + 1);
        const QString name = "page" + id + ".png";
        success = archive.add(("word/media/" + name).toUtf8(), page.png);
        relationships += QStringLiteral(
            "<Relationship Id=\"rId%1\" "
            "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" "
            "Target=\"media/%2\"/>").arg(id, name);
        document += QStringLiteral("<w:p><w:pPr><w:spacing w:before=\"0\" w:after=\"0\"/>");
        if (index > 0) document += QStringLiteral("<w:pageBreakBefore/>");
        document += QStringLiteral(
            "</w:pPr><w:r><w:drawing><wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\">"
            "<wp:extent cx=\"%1\" cy=\"%2\"/><wp:docPr id=\"%3\" name=\"%4\"/>"
            "<wp:cNvGraphicFramePr><a:graphicFrameLocks noChangeAspect=\"1\"/></wp:cNvGraphicFramePr>"
            "<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
            "<pic:pic><pic:nvPicPr><pic:cNvPr id=\"%3\" name=\"%4\"/><pic:cNvPicPr/></pic:nvPicPr>"
            "<pic:blipFill><a:blip r:embed=\"rId%3\"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill>"
            "<pic:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"%1\" cy=\"%2\"/></a:xfrm>"
            "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></pic:spPr>"
            "</pic:pic></a:graphicData></a:graphic></wp:inline></w:drawing></w:r></w:p>")
            .arg(cx).arg(cy).arg(id, name);
    }
    relationships += QStringLiteral("</Relationships>");
    document += QStringLiteral(
        "<w:sectPr><w:pgSz w:w=\"11906\" w:h=\"16838\"/>"
        "<w:pgMar w:top=\"1020\" w:right=\"1020\" w:bottom=\"1020\" w:left=\"1020\" "
        "w:header=\"0\" w:footer=\"0\" w:gutter=\"0\"/></w:sectPr></w:body></w:document>");
    success = success && archive.add("word/document.xml", document.toUtf8())
              && archive.add("word/_rels/document.xml.rels", relationships.toUtf8());
    const bool closed = archive.close();
    if (!success || !closed) {
        if (error) *error = QStringLiteral("DOCX 内容写入失败，未替换目标文件。");
        return false;
    }
    QFile source(temporary.fileName());
    QSaveFile target(path);
    if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("无法保存 DOCX：%1").arg(
            source.isOpen() ? target.errorString() : source.errorString());
        return false;
    }
    while (!source.atEnd()) {
        const QByteArray chunk = source.read(1024 * 1024);
        if (source.error() != QFileDevice::NoError || target.write(chunk) != chunk.size()) {
            if (error) *error = QStringLiteral("DOCX 写入失败。请检查磁盘空间与文件权限。");
            return false;
        }
    }
    if (!target.commit()) {
        if (error) *error = QStringLiteral("无法完成 DOCX 保存：%1").arg(target.errorString());
        return false;
    }
    return true;
}

std::unique_ptr<QMimeData> annotationClipboard(const QVector<AnnotationPage> &pages)
{
    auto mime = std::make_unique<QMimeData>();
    QString html = QStringLiteral("<html><body>");
    QByteArray rtf = "{\\rtf1\\ansi\\deff0\\paperw11906\\paperh16838"
                     "\\margl1020\\margr1020\\margt1020\\margb1020\n";
    for (int index = 0; index < pages.size(); ++index) {
        const auto &page = pages.at(index);
        const QSizeF size = printSize(page);
        html += QStringLiteral("<p style=\"margin:0;%1\"><img alt=\"Annotation page %2\" "
                               "width=\"%3\" height=\"%4\" style=\"width:%5pt;height:%6pt;\" "
                               "src=\"data:image/png;base64,%7\"/></p>")
            .arg(index > 0 ? QStringLiteral("page-break-before:always") : QString())
            .arg(index + 1).arg(qRound(size.width())).arg(qRound(size.height()))
            .arg(size.width() * 0.75, 0, 'f', 2).arg(size.height() * 0.75, 0, 'f', 2)
            .arg(QString::fromLatin1(page.png.toBase64()));
        if (index > 0) rtf += "\\page\n";
        rtf += "\\pard\\sa0\\sb0{\\pict\\pngblip\\picw"
               + QByteArray::number(qRound(page.size.width() * 3))
               + "\\pich" + QByteArray::number(qRound(page.size.height() * 3))
               + "\\picwgoal"
               + QByteArray::number(qRound(size.width() * 15))
               + "\\pichgoal" + QByteArray::number(qRound(size.height() * 15))
               + "\n" + page.png.toHex() + "}\\par\n";
    }
    html += QStringLiteral("</body></html>");
    rtf += "}";
    mime->setHtml(html);
    mime->setData("text/rtf", rtf);
    mime->setData("application/rtf", rtf);
#ifdef Q_OS_WIN
    mime->setData("application/x-qt-windows-mime;value=\"Rich Text Format\"", rtf);
#endif
    return mime;
}
