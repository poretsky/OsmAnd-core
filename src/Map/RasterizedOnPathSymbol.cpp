#include "RasterizedOnPathSymbol.h"

OsmAnd::RasterizedOnPathSymbol::RasterizedOnPathSymbol(
    const std::shared_ptr<const RasterizedSymbolsGroup>& group_,
    const std::shared_ptr<const Model::BinaryMapObject>& mapObject_,
    const std::shared_ptr<const SkBitmap>& bitmap_,
    const int order_,
    const QString& content_,
    const LanguageId& languageId_,
    const PointI& minDistance_,
    const QVector<SkScalar>& glyphsWidth_)
    : RasterizedSymbol(group_, mapObject_, bitmap_, order_, content_, languageId_, minDistance_)
    , glyphsWidth(glyphsWidth_)
{
}

OsmAnd::RasterizedOnPathSymbol::~RasterizedOnPathSymbol()
{
}
