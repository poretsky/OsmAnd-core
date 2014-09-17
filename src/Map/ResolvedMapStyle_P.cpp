#include "ResolvedMapStyle_P.h"
#include "ResolvedMapStyle.h"

#include "QtExtensions.h"
#include "ignore_warnings_on_external_includes.h"
#include "restore_internal_warnings.h"
#include "QtCommon.h"

#include "MapStyleValueDefinition.h"
#include "MapStyleBuiltinValueDefinitions.h"
#include "QKeyValueIterator.h"
#include "Logging.h"

OsmAnd::ResolvedMapStyle_P::ResolvedMapStyle_P(ResolvedMapStyle* const owner_)
    : owner(owner_)
{
}

OsmAnd::ResolvedMapStyle_P::~ResolvedMapStyle_P()
{
}

OsmAnd::ResolvedMapStyle_P::StringId OsmAnd::ResolvedMapStyle_P::addStringToLUT(const QString& value)
{
    const auto newId = static_cast<OsmAnd::ResolvedMapStyle_P::StringId>(_stringsForwardLUT.size());

    _stringsBackwardLUT.insert(value, newId);
    _stringsForwardLUT.push_back(value);

    return newId;
}

OsmAnd::ResolvedMapStyle_P::StringId OsmAnd::ResolvedMapStyle_P::resolveStringIdInLUT(const QString& value)
{
    const auto citStringId = _stringsBackwardLUT.constFind(value);
    if (citStringId == _stringsBackwardLUT.cend())
        return addStringToLUT(value);
    return *citStringId;
}


bool OsmAnd::ResolvedMapStyle_P::resolveStringIdInLUT(const QString& value, StringId& outId) const
{
    const auto citStringId = _stringsBackwardLUT.constFind(value);
    if (citStringId == _stringsBackwardLUT.cend())
        return false;

    outId = *citStringId;
    return true;
}

bool OsmAnd::ResolvedMapStyle_P::parseValue(const QString& input, const MapStyleValueDataType dataType, const bool isComplex, MapStyleValue& outValue)
{
    if (input.startsWith(QLatin1Char('$')))
    {
        const auto constantName = input.mid(1);
        const auto citConstantValue = _constants.constFind(constantName);
        if (citConstantValue == _constants.cend())
        {
            LogPrintf(LogSeverityLevel::Error,
                "Constant '%s' not defined",
                qPrintable(constantName));
            return false;
        }
        const auto& constantValue = *citConstantValue;

        return parseValue(constantValue, dataType, isComplex, outValue);
    }

    if (dataType == MapStyleValueDataType::String)
    {
        outValue.asSimple.asUInt = resolveStringIdInLUT(input);
        return true;
    }
    
    return MapStyleValue::parse(input, dataType, isComplex, outValue);
}

bool OsmAnd::ResolvedMapStyle_P::parseValue(const QString& input, const MapStyleValueDataType dataType, const bool isComplex, MapStyleValue& outValue) const
{
    if (input.startsWith(QLatin1Char('$')))
    {
        const auto constantName = input.mid(1);
        const auto citConstantValue = _constants.constFind(constantName);
        if (citConstantValue == _constants.cend())
        {
            LogPrintf(LogSeverityLevel::Error,
                "Constant '%s' not defined",
                qPrintable(constantName));
            return false;
        }
        const auto& constantValue = *citConstantValue;

        return parseValue(constantValue, dataType, isComplex, outValue);
    }

    if (dataType == MapStyleValueDataType::String)
         return resolveStringIdInLUT(input, outValue.asSimple.asUInt);

    return MapStyleValue::parse(input, dataType, isComplex, outValue);
}

void OsmAnd::ResolvedMapStyle_P::registerBuiltinValueDefinitions()
{
    const auto builtinValueDefs = MapStyleBuiltinValueDefinitions::get();

    // Index of built-in value definition in _valuesDefinitions list always match it's id
#   define DECLARE_BUILTIN_VALUEDEF(varname, valueClass, dataType, name, isComplex)                             \
        _valuesDefinitions.push_back(builtinValueDefs->varname);                                                \
        _valuesDefinitionsIndicesByName.insert(QLatin1String(name), builtinValueDefs->varname->id);             \
        assert(_valuesDefinitions.size() - 1 == builtinValueDefs->varname->id);
#   include "MapStyleBuiltinValueDefinitions_Set.h"
#   undef DECLARE_BUILTIN_VALUEDEF
}

bool OsmAnd::ResolvedMapStyle_P::collectConstants()
{
    // Process styles chain in reverse order, top-most parent is the last element
    auto citUnresolvedMapStyle = iteratorOf(owner->unresolvedMapStylesChain);
    citUnresolvedMapStyle.toBack();
    while (citUnresolvedMapStyle.hasPrevious())
    {
        const auto& unresolvedMapStyle = citUnresolvedMapStyle.previous();

        for (const auto& constantEntry : rangeOf(constOf(unresolvedMapStyle->constants)))
            _constants[constantEntry.key()] = constantEntry.value();
    }

    return true;
}

std::shared_ptr<OsmAnd::ResolvedMapStyle_P::RuleNode> OsmAnd::ResolvedMapStyle_P::resolveRuleNode(
    const std::shared_ptr<const UnresolvedMapStyle::RuleNode>& unresolvedRuleNode)
{
    const std::shared_ptr<RuleNode> resolvedRuleNode(new RuleNode());

    // Resolve values
    for (const auto& itUnresolvedValueEntry : rangeOf(constOf(unresolvedRuleNode->values)))
    {
        const auto& name = itUnresolvedValueEntry.key();
        const auto& value = itUnresolvedValueEntry.value();

        // Find value definition id and object
        const auto valueDefId = getValueDefinitionIdByName(name);
        const auto valueDef = getValueDefinitionById(valueDefId);
        if (valueDefId < 0 || !valueDef)
        {
            LogPrintf(LogSeverityLevel::Warning,
                "Ignoring unknown value '%s' = '%s'",
                qPrintable(name),
                qPrintable(value));
            continue;
        }

        // Try to parse value
        MapStyleValue parsedValue;
        if (!parseValue(value, valueDef->dataType, valueDef->isComplex, parsedValue))
        {
            LogPrintf(LogSeverityLevel::Warning,
                "Ignoring value for '%s' since '%s' can not be parsed correctly",
                qPrintable(name),
                qPrintable(value));
            continue;
        }
        
        resolvedRuleNode->values[valueDefId] = parsedValue;
    }

    // <switch>/<case> subnodes
    for (const auto& unresolvedChild : constOf(unresolvedRuleNode->oneOfConditionalSubnodes))
    {
        const auto resolvedChild = resolveRuleNode(unresolvedChild);
        if (!resolvedChild)
            return nullptr;

        resolvedRuleNode->oneOfConditionalSubnodes.push_back(resolvedChild);
    }

    // <apply> subnodes
    for (const auto& unresolvedChild : constOf(unresolvedRuleNode->applySubnodes))
    {
        const auto resolvedChild = resolveRuleNode(unresolvedChild);
        if (!resolvedChild)
            return nullptr;

        resolvedRuleNode->applySubnodes.push_back(resolvedChild);
    }

    return resolvedRuleNode;
}

bool OsmAnd::ResolvedMapStyle_P::mergeAndResolveParameters()
{
    // Process styles chain in direct order, to exclude redefined parameters
    auto citUnresolvedMapStyle = iteratorOf(owner->unresolvedMapStylesChain);
    while (citUnresolvedMapStyle.hasNext())
    {
        const auto& unresolvedMapStyle = citUnresolvedMapStyle.next();

        for (const auto& unresolvedParameter : constOf(unresolvedMapStyle->parameters))
        {
            const auto nameId = resolveStringIdInLUT(unresolvedParameter->name);
            auto& resolvedParameter = _parameters[nameId];

            // Skip already defined parameters
            if (resolvedParameter)
                continue;

            // Resolve possible values
            QList<MapStyleValue> resolvedPossibleValues;
            for (const auto& possibleValue : constOf(unresolvedParameter->possibleValues))
            {
                MapStyleValue resolvedPossibleValue;
                if (!parseValue(possibleValue, unresolvedParameter->dataType, false, resolvedPossibleValue))
                {
                    LogPrintf(LogSeverityLevel::Error,
                        "Failed to parse '%s' as possible value for '%s' (%s)",
                        qPrintable(possibleValue),
                        qPrintable(unresolvedParameter->name),
                        qPrintable(unresolvedParameter->title));
                    return false;
                }

                resolvedPossibleValues.push_back(qMove(resolvedPossibleValue));
            }

            // Create new resolved parameter
            resolvedParameter.reset(new Parameter(
                unresolvedParameter->title,
                unresolvedParameter->description,
                nameId,
                unresolvedParameter->dataType,
                resolvedPossibleValues));

            // Register parameter as value definition
            const auto newValueDefId = _valuesDefinitions.size();
            const std::shared_ptr<ParameterValueDefinition> inputValueDefinition(new ParameterValueDefinition(
                newValueDefId,
                unresolvedParameter->name,
                resolvedParameter));
            _valuesDefinitions.push_back(inputValueDefinition);
            _valuesDefinitionsIndicesByName.insert(unresolvedParameter->name, newValueDefId);
        }
    }

    return true;
}

bool OsmAnd::ResolvedMapStyle_P::mergeAndResolveAttributes()
{
    QHash<StringId, std::shared_ptr<Attribute> > attributes;

    // Process styles chain in reverse order, top-most parent is the last element
    auto citUnresolvedMapStyle = iteratorOf(owner->unresolvedMapStylesChain);
    citUnresolvedMapStyle.toBack();
    while (citUnresolvedMapStyle.hasPrevious())
    {
        const auto& unresolvedMapStyle = citUnresolvedMapStyle.previous();

        for (const auto& unresolvedAttribute : constOf(unresolvedMapStyle->attributes))
        {
            const auto nameId = resolveStringIdInLUT(unresolvedAttribute->name);
            auto& resolvedAttribute = attributes[nameId];

            // Create resolved attribute if needed
            if (!resolvedAttribute)
                resolvedAttribute.reset(new Attribute(nameId));

            const auto resolvedRuleNode = resolveRuleNode(unresolvedAttribute->rootNode);
            resolvedAttribute->rootNode->oneOfConditionalSubnodes.append(resolvedRuleNode->oneOfConditionalSubnodes);
        }
    }

    _attributes = copyAs< StringId, std::shared_ptr<const Attribute> >(attributes);

    return true;
}

bool OsmAnd::ResolvedMapStyle_P::mergeAndResolveRulesets()
{
    const auto builtinValueDefs = MapStyleBuiltinValueDefinitions::get();

    for (auto rulesetTypeIdx = 0u; rulesetTypeIdx < MapStyleRulesetTypesCount; rulesetTypeIdx++)
    {
        QHash<uint64_t, std::shared_ptr<Rule> > ruleset;

        // Process styles chain in direct order. This will allow to process overriding correctly
        auto citUnresolvedMapStyle = iteratorOf(owner->unresolvedMapStylesChain);
        while (citUnresolvedMapStyle.hasNext())
        {
            const auto& unresolvedMapStyle = citUnresolvedMapStyle.next();

            const auto& unresolvedRuleset = unresolvedMapStyle->rulesets[rulesetTypeIdx];
            auto itUnresolvedByTag = iteratorOf(unresolvedRuleset);
            while (itUnresolvedByTag.hasNext())
            {
                const auto& unresolvedByTagEntry = itUnresolvedByTag.next();
                const auto& tag = unresolvedByTagEntry.key();
                const auto& unresolvedByTag = unresolvedByTagEntry.value();

                const auto tagId = resolveStringIdInLUT(tag);

                auto itUnresolvedByValue = iteratorOf(unresolvedByTag);
                while (itUnresolvedByValue.hasNext())
                {
                    const auto& unresolvedByValueEntry = itUnresolvedByValue.next();
                    const auto& value = unresolvedByValueEntry.key();
                    const auto& unresolvedRule = unresolvedByValueEntry.value();

                    const auto valueId = resolveStringIdInLUT(value);

                    const auto tagValueId = ResolvedMapStyle::composeTagValueId(tagId, valueId);
                    auto& topLevelRule = ruleset[tagValueId];
                    
                    // Create top-level rule if not yet created
                    if (!topLevelRule)
                    {
                        topLevelRule.reset(new Rule(static_cast<MapStyleRulesetType>(rulesetTypeIdx)));

                        topLevelRule->rootNode->values[builtinValueDefs->id_INPUT_TAG] = MapStyleValue::fromSimpleUInt(tagId);
                        topLevelRule->rootNode->values[builtinValueDefs->id_INPUT_VALUE] = MapStyleValue::fromSimpleUInt(valueId);
                    }

                    const auto resolvedRuleNode = resolveRuleNode(unresolvedRule->rootNode);
                    if (!resolvedRuleNode)
                    {
                        LogPrintf(LogSeverityLevel::Warning,
                            "One of rules for '%s':'%s' was ignored due to failure to resolve it",
                            qPrintable(tag),
                            qPrintable(value));
                        return false;
                    }
                    topLevelRule->rootNode->oneOfConditionalSubnodes.append(resolvedRuleNode->oneOfConditionalSubnodes);
                }
            }

            _rulesets[rulesetTypeIdx] = copyAs< uint64_t, std::shared_ptr<const Rule> >(ruleset);
        }
    }

    return true;
}

bool OsmAnd::ResolvedMapStyle_P::resolve()
{
    // Empty string always have 0 identifier
    addStringToLUT(QString());

    if (!collectConstants())
        return false;

    // Register built-in value definitions first
    registerBuiltinValueDefinitions();
    if (!mergeAndResolveParameters())
        return false;

    if (!mergeAndResolveAttributes())
        return false;

    if (!mergeAndResolveRulesets())
        return false;

    return true;
}

OsmAnd::ResolvedMapStyle_P::ValueDefinitionId OsmAnd::ResolvedMapStyle_P::getValueDefinitionIdByName(const QString& name) const
{
    const auto citId = _valuesDefinitionsIndicesByName.constFind(name);
    if (citId == _valuesDefinitionsIndicesByName.cend())
        return -1;
    return *citId;
}

std::shared_ptr<const OsmAnd::MapStyleValueDefinition> OsmAnd::ResolvedMapStyle_P::getValueDefinitionById(const ValueDefinitionId id) const
{
    if (id < 0 || id >= _valuesDefinitions.size())
        return nullptr;
    return _valuesDefinitions[id];
}

bool OsmAnd::ResolvedMapStyle_P::parseValue(const QString& input, const ValueDefinitionId valueDefintionId, MapStyleValue& outParsedValue) const
{
    if (valueDefintionId < 0 || valueDefintionId >= _valuesDefinitions.size())
        return false;
    return parseValue(input, _valuesDefinitions[valueDefintionId], outParsedValue);
}

bool OsmAnd::ResolvedMapStyle_P::parseValue(const QString& input, const std::shared_ptr<const MapStyleValueDefinition>& valueDefintion, MapStyleValue& outParsedValue) const
{
    return parseValue(input, valueDefintion->dataType, valueDefintion->isComplex, outParsedValue);
}

std::shared_ptr<const OsmAnd::ResolvedMapStyle_P::Attribute> OsmAnd::ResolvedMapStyle_P::getAttribute(const QString& name) const
{
    StringId nameId;
    if (!resolveStringIdInLUT(name, nameId))
        return nullptr;

    return _attributes[nameId];
}

const QHash< OsmAnd::ResolvedMapStyle_P::TagValueId, std::shared_ptr<const OsmAnd::ResolvedMapStyle_P::Rule> >
OsmAnd::ResolvedMapStyle_P::getRuleset(
    const MapStyleRulesetType rulesetType) const
{
    return _rulesets[static_cast<unsigned int>(rulesetType)];
}

QString OsmAnd::ResolvedMapStyle_P::getStringById(const StringId id) const
{
    if (id >= _stringsForwardLUT.size())
        return QString::null;
    return _stringsForwardLUT[id];
}