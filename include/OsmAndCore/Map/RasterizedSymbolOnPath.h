#ifndef _OSMAND_CORE_RASTERIZED_SYMBOL_ON_PATH_H_
#define _OSMAND_CORE_RASTERIZED_SYMBOL_ON_PATH_H_

#include <OsmAndCore/stdlib_common.h>

#include <OsmAndCore/QtExtensions.h>
#include <QList>

#include <OsmAndCore.h>
#include <OsmAndCore/CommonTypes.h>
#include <OsmAndCore/Map/RasterizedSymbol.h>

namespace OsmAnd
{
    class Rasterizer_P;

    class OSMAND_CORE_API RasterizedSymbolOnPath : public RasterizedSymbol
    {
        Q_DISABLE_COPY(RasterizedSymbolOnPath);
    private:
    protected:
        RasterizedSymbolOnPath(
            const std::shared_ptr<const RasterizedSymbolsGroup>& group,
            const std::shared_ptr<const Model::MapObject>& mapObject,
            const std::shared_ptr<const SkBitmap>& bitmap,
            const int order,
            const QString& content,
            const LanguageId& languageId,
            const PointI& minDistance,
            const QVector<SkScalar>& glyphsWidth);
    public:
        virtual ~RasterizedSymbolOnPath();

        const QVector<SkScalar> glyphsWidth;

        friend class OsmAnd::Rasterizer_P;
    };

} // namespace OsmAnd

#endif // !defined(_OSMAND_CORE_RASTERIZED_SYMBOL_ON_PATH_H_)
