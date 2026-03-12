/* -*- Mode: C++; indent-tabs-mode: nil; tab-width: 4 -*-
 * -*- coding: utf-8 -*-
 *
 * Copyright (C) 2012 by Alejandro Fiestas Olivares <afiestas@kde.org>
 * Copyright 2016 by Sebastian Kügler <sebas@kde.org>
 * Copyright (c) 2018 Kai Uwe Broulik <kde@broulik.de>
 *                    Work sponsored by the LiMux project of
 *                    the city of Munich.
 * Copyright (C) 2020 KylinSoft Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "xrandr-output.h"
#include <QStringList>
#include <QFile>
#include <QJsonDocument>
#include <QDir>
#include <QLoggingCategory>
#include <QRect>
#include <QStandardPaths>
#include <QtXml>
#include <KF5/KScreen/kscreen/output.h>
#include <KF5/KScreen/kscreen/edid.h>
#include "xrandr-config.h"
#include "clib-syslog.h"

QString xrandrOutput::mDirName = QStringLiteral("outputs/");

QString xrandrOutput::dirPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) %
            QStringLiteral("/kscreen/") % mDirName;
}

QString xrandrOutput::globalFileName(const QString &hash)
{
//    const auto dir = dirPath();
//    if (!QDir().mkpath(dir)) {
//     USD_LOG(LOG_DEBUG,":::::::::::::::::::::::::::::::::::::::::::::::::::::::::[%s],dir:[%s]",QString().toLatin1().data(), dirPath().toLatin1().data());
//        return QString();
//    }

//    USD_LOG(LOG_DEBUG,":::::::::::::::::::::::::::::::::::::::::::::::::::::::::[%s],dir:[%s]",QString().toLatin1().data(), dirPath().toLatin1().data());
    return QString();//dir % hash;
}

bool xrandrOutput::readInGlobalPartFromInfo(KScreen::OutputPtr output, const QVariantMap &info)
{
    output->setRotation(static_cast<KScreen::Output::Rotation>(info.value(QStringLiteral("rotation"), 1).toInt()));

    bool scaleOk;
    bool dpiok;
    const qreal scale = info.value(QStringLiteral("scale"), 1).toDouble(&scaleOk);

    if (scaleOk) {
        output->setScale(1);//old config file...
    } else {
        output->setScale(1);
    }

//    //兼容分数缩放
//    const int dpi = info.value(QStringLiteral("dpi"), 1).toInt(&dpiok);
//    if (!dpiok) {//没有缩放的配置则需要设置为0.5。如果存了dpi就按照scale进行设置
//        if (UsdBaseClass::getDPI() == 192 && scale == 1) {
//            QPoint point(output->pos().x()/0.5,output->pos().y()/0.5);
//            output->setScale(0.5);//todo:x,y坐标调整
//            output->setPos(point);
//        }
//    }

    const QVariantMap modeInfo = info[QStringLiteral("mode")].toMap();
    const QVariantMap modeSize = modeInfo[QStringLiteral("size")].toMap();
    const QSize size = QSize(modeSize[QStringLiteral("width")].toInt(), modeSize[QStringLiteral("height")].toInt());
    //qDebug() << "Finding a mode for" << size << "@" << modeInfo[QStringLiteral("refresh")].toFloat();

    const KScreen::ModeList modes = output->modes();
    KScreen::ModePtr matchingMode;

    if (modes.count()<1) {
        USD_LOG(LOG_DEBUG, "%s mode count = 0.",output->name().toLatin1().data());
    }

    for (const KScreen::ModePtr &mode : modes) {
        if (mode->size() != size) {

            continue;
        }
        if (!qFuzzyCompare(mode->refreshRate(), modeInfo[QStringLiteral("refresh")].toFloat())) {
            continue;
        }
        matchingMode = mode;
        break;
    }

    if (!matchingMode) {
        uint outScreenMaxSize = 0;
        float refresh = 0;

        /*if (output->preferredMode() != nullptr) {//fuck jjw7200 断开最后的VGA这个preferredmode访问就出错。
            USD_LOG(LOG_ERR,"Failed to %s find a matching mode - this means that our config is corrupted reset it to preferredModeId...",output->name().toLatin1().data());
            matchingMode = output->preferredMode();
            USD_LOG(LOG_ERR,"Failed to %s find a matching mode - this means that our config is corrupted reset it to preferredModeId...",output->name().toLatin1().data());
        } else */{
            for (const KScreen::ModePtr &mode : modes) {
                if (mode->size().height() + mode->size().width() > outScreenMaxSize) {
                    outScreenMaxSize = mode->size().height() + mode->size().width();
                    matchingMode = mode;
                    refresh = mode->refreshRate();
                    USD_LOG(LOG_ERR,"Failed to %s find a matching mode - this means that our config is corrupted reset it :%d...",output->name().toLatin1().data(),outScreenMaxSize);
                } else if (mode->size().height() + mode->size().width() == outScreenMaxSize && mode->refreshRate() > refresh){
                    matchingMode = mode;
                    refresh = mode->refreshRate();
                    USD_LOG(LOG_ERR,"Failed to %s find a matching mode - this means that our config is corrupted reset it.%d...",output->name().toLatin1().data(),outScreenMaxSize);
                }
            }
        }
    }

    if (!matchingMode) {
        USD_LOG(LOG_DEBUG,"Failed to %s find a matching mode - this means that our config is corrupted",output->name().toLatin1().data());
//        matchingMode = output->preferredMode();
         output->setEnabled(false);
    }
    if (!matchingMode) {//没有找到最适合分辨率
          USD_LOG(LOG_DEBUG,"Failed to get a preferred mode, falling back to biggest mode.");
//        matchingMode = Generator::biggestMode(modes);
    }
    if (!matchingMode) {
        USD_LOG(LOG_DEBUG,"Failed to get biggest mode. Which means there are no modes. Turning off the screen.");
        output->setEnabled(false);
        return false;
    }

    output->setCurrentModeId(matchingMode->id());
    return true;
}

QVariantMap xrandrOutput::getGlobalData(KScreen::OutputPtr output)
{
    QFile file(globalFileName(output->hashMd5()));
    if (!file.exists()) {
        return QVariantMap();
    }
    if (!file.open(QIODevice::ReadOnly)) {
//        USD_LOG(LOG_DEBUG, "Failed to open file %s" , file.fileName().toLatin1().data());
        return QVariantMap();
    }
    QJsonDocument parser;
    return parser.fromJson(file.readAll()).toVariant().toMap();
}

bool xrandrOutput::readInGlobal(KScreen::OutputPtr output)
{
    const QVariantMap info = getGlobalData(output);
    if (info.empty()) {
//        USD_LOG(LOG_DEBUG,"can't get info...");
        // if info is empty, the global file does not exists, or is in an unreadable state
        return false;
    }
    return readInGlobalPartFromInfo(output, info);
}

// TODO: move this into the Layouter class.
void xrandrOutput::adjustPositions(KScreen::ConfigPtr config, const QVariantList &outputsInfo)
{
    typedef QPair<int, QPoint> Out;

      KScreen::OutputList outputs = config->outputs();
      QVector<Out> sortedOutputs; // <id, pos>
      for (const KScreen::OutputPtr &output : outputs) {
          sortedOutputs.append(Out(output->id(), output->pos()));
      }

      // go from left to right, top to bottom
      std::sort(sortedOutputs.begin(), sortedOutputs.end(), [](const Out &o1, const Out &o2) {
          const int x1 = o1.second.x();
          const int x2 = o2.second.x();
          return x1 < x2 || (x1 == x2 && o1.second.y() < o2.second.y());
      });

      for (int cnt = 1; cnt < sortedOutputs.length(); cnt++) {
          auto getOutputInfoProperties = [outputsInfo](KScreen::OutputPtr output, QRect &geo) -> bool {
              if (!output) {
                  return false;
              }
              const auto hash = output->hash();
              const auto name = output->name();

              auto it = std::find_if(outputsInfo.begin(), outputsInfo.end(),
                  [hash,name](QVariant v) {
                      const QVariantMap info = v.toMap();
                      const QVariantMap metadata = info[QStringLiteral("pos")].toMap();
                      return (info[QStringLiteral("id")].toString() == hash) && (metadata[QStringLiteral("name")].toString() == name);
                  }
              );
              if (it == outputsInfo.end()) {
                  return false;
              }

              auto isPortrait = [](const QVariant &info) {
                  bool ok;
                  const int rot = info.toInt(&ok);
                  if (!ok) {
                      return false;
                  }
                  return rot & KScreen::Output::Rotation::Left ||
                          rot & KScreen::Output::Rotation::Right;
              };

              const QVariantMap outputInfo = it->toMap();

              const QVariantMap posInfo = outputInfo[QStringLiteral("pos")].toMap();
              const QVariant scaleInfo = outputInfo[QStringLiteral("scale")];
              const QVariantMap modeInfo = outputInfo[QStringLiteral("mode")].toMap();
              const QVariantMap modeSize = modeInfo[QStringLiteral("size")].toMap();
              const bool portrait = isPortrait(outputInfo[QStringLiteral("rotation")]);

              if (posInfo.isEmpty() || modeSize.isEmpty() || !scaleInfo.canConvert<int>()) {
                  return false;
              }

              const qreal scale = scaleInfo.toDouble();
              if (scale <= 0) {
                  return false;
              }
              const QPoint pos = QPoint(posInfo[QStringLiteral("x")].toInt(), posInfo[QStringLiteral("y")].toInt());
              QSize size = QSize(modeSize[QStringLiteral("width")].toInt() / scale,
                                 modeSize[QStringLiteral("height")].toInt() / scale);
              if (portrait) {
                  size.transpose();
              }
              geo = QRect(pos, size);

              return true;
          };

          // it's guaranteed that we find the following values in the QMap
          KScreen::OutputPtr prevPtr = outputs.find(sortedOutputs[cnt - 1].first).value();
          KScreen::OutputPtr curPtr = outputs.find(sortedOutputs[cnt].first).value();

          QRect prevInfoGeo, curInfoGeo;
          if (!getOutputInfoProperties(prevPtr, prevInfoGeo) ||
                  !getOutputInfoProperties(curPtr, curInfoGeo)) {
              // no info found, nothing can be adjusted for the next output
              continue;
          }

          const QRect prevGeo = prevPtr->geometry();
          const QRect curGeo = curPtr->geometry();

          // the old difference between previous and current output read from the config file
          const int xInfoDiff = curInfoGeo.x() - (prevInfoGeo.x() + prevInfoGeo.width());

          // the proposed new difference
          const int prevRight = prevGeo.x() + prevGeo.width();
          const int xCorrected = prevRight + prevGeo.width() * xInfoDiff / (double)prevInfoGeo.width();
          const int xDiff = curGeo.x() - prevRight;

          // In the following calculate the y-correction. This is more involved since we
          // differentiate between overlapping and non-overlapping pairs and align either
          // top to top/bottom or bottom to top/bottom
          const bool yOverlap = prevInfoGeo.y() + prevInfoGeo.height() > curInfoGeo.y() &&
                  prevInfoGeo.y() < curInfoGeo.y() + curInfoGeo.height();

          // these values determine which horizontal edge of previous output we align with
          const int topToTopDiffAbs = qAbs(prevInfoGeo.y() - curInfoGeo.y());
          const int topToBottomDiffAbs = qAbs(prevInfoGeo.y() - curInfoGeo.y() - curInfoGeo.height());
          const int bottomToBottomDiffAbs = qAbs(prevInfoGeo.y() + prevInfoGeo.height() - curInfoGeo.y() - curInfoGeo.height());
          const int bottomToTopDiffAbs = qAbs(prevInfoGeo.y() + prevInfoGeo.height() - curInfoGeo.y());

          const bool yTopAligned = (topToTopDiffAbs < bottomToBottomDiffAbs && topToTopDiffAbs <= bottomToTopDiffAbs) ||
                  topToBottomDiffAbs < bottomToBottomDiffAbs;

          int yInfoDiff = curInfoGeo.y() - prevInfoGeo.y();
          int yDiff = curGeo.y() - prevGeo.y();
          int yCorrected;

          if (yTopAligned) {
              // align to previous top
              if (!yOverlap) {
                  // align previous top with current bottom
                  yInfoDiff += curInfoGeo.height();
                  yDiff += curGeo.height();
              }
              // When we align with previous top we are interested in the changes to the
              // current geometry and not in the ones of the previous one.
              const double yInfoRel = yInfoDiff / (double)curInfoGeo.height();
              yCorrected = prevGeo.y() + yInfoRel * curGeo.height();
          } else {
              // align previous bottom...
              yInfoDiff -= prevInfoGeo.height();
              yDiff -= prevGeo.height();
              yCorrected = prevGeo.y() + prevGeo.height();

              if (yOverlap) {
                  // ... with current bottom
                  yInfoDiff += curInfoGeo.height();
                  yDiff += curGeo.height();
                  yCorrected -= curGeo.height();
              } // ... else with current top

              // When we align with previous bottom we are interested in changes to the
              // previous geometry.
              const double yInfoRel = yInfoDiff / (double)prevInfoGeo.height();
              yCorrected += yInfoRel * prevGeo.height();
          }

          const int x = xDiff == xInfoDiff ? curGeo.x() : xCorrected;
          const int y = yDiff == yInfoDiff ? curGeo.y() : yCorrected;
          curPtr->setPos(QPoint(x, y));

      }

}


bool xrandrOutput::readIn(KScreen::OutputPtr output, const QVariantMap &info)
{

    const QVariantMap posInfo = info[QStringLiteral("pos")].toMap();
    QPoint point(posInfo[QStringLiteral("x")].toInt(), posInfo[QStringLiteral("y")].toInt());
    output->setPos(point);
    output->setPrimary(info[QStringLiteral("primary")].toBool());
    output->setEnabled(info[QStringLiteral("enabled")].toBool());

    if (!output->isEnabled()) {
        return true;
    }
    // output data read directly from info
    return readInGlobalPartFromInfo(output, info);
}

bool xrandrOutput::readInOutputs(KScreen::ConfigPtr config, const QVariantList &outputsInfo)
{
    const KScreen::OutputList outputs = config->outputs();

    int connectedCount = 0;
    int readCount = 0;

    bool isSameHash = false;//for bug #140084,#140081
    QStringList qstrList;

    for (const KScreen::OutputPtr &output : outputs) {
        if (!output->isConnected()) {
            continue;
        }
        if (qstrList.contains(output->hash())) {
            isSameHash = true;
            break;
        }
        qstrList.append(output->hash());
    }

    for (const KScreen::OutputPtr &output : outputs) {
        if (!output->isConnected()) {
            output->setEnabled(false);
            continue;
        }

        connectedCount++;
        const auto outputId = output->hash();
        bool infoFound = false;

        for (const auto &variantInfo : outputsInfo) {
            const QVariantMap info = variantInfo.toMap();
            if (outputId != info[QStringLiteral("id")].toString()) {
                USD_LOG(LOG_DEBUG,"%s != %s",outputId.toLatin1().data(), info[QStringLiteral("id")].toString().toLatin1().data());
                continue;
            } else {
                USD_LOG(LOG_DEBUG,"find %s:%s",output->name().toLatin1().data(), outputId.toLatin1().data());
            }

            if (!output->name().isEmpty()) {
                // We may have identical outputs connected, these will have the same id in the config
                // in order to find the right one, also check the output's name (usually the connector)
                const auto metadata = info[QStringLiteral("metadata")].toMap();
                const auto outputName = metadata[QStringLiteral("name")].toString();

                if (isSameHash && output->name() != outputName) {
                    continue;
                }

                if (readIn(output, info)) {
                    USD_LOG(LOG_DEBUG,"name:%s::outputName:%s",output->name().toLatin1().data(), outputName.toLatin1().data());
                    infoFound = true;
                    readCount++;
                }
            }
        }
        if (!infoFound) {
            // no info in info for this output, try reading in global output info at least or set some default values
            USD_LOG(LOG_DEBUG,"Failed to find a matching output in the current info data - this means that our info is corrupted or a different device with the same serial number has been connected (very unlikely).");
        }
    }

    if (connectedCount != readCount) {
        return false;
    }
    // TODO: this does not work at the moment with logical size replication. Deactivate for now.
    // correct positional config regressions on global output data changes
#if 1
    adjustPositions(config, outputsInfo);
#endif
    return true;
}

QVariantMap metadata(const KScreen::OutputPtr &output)
{
    QVariantMap metadata;
    metadata[QStringLiteral("name")] = output->name();
    if (!output->edid() || !output->edid()->isValid()) {
        return metadata;
    }

    metadata[QStringLiteral("fullname")] = output->edid()->deviceId();
    return metadata;
}

void xrandrOutput::writeGlobal(const KScreen::OutputPtr &output)
{
    // get old values and subsequently override
    QVariantMap info = getGlobalData(output);
    if (!writeGlobalPart(output, info, nullptr)) {
        USD_LOG(LOG_DEBUG,"write global part faile..");
        return;
    }

    QFile file(globalFileName(output->hashMd5()));
    if (!file.open(QIODevice::WriteOnly)) {
        USD_LOG(LOG_DEBUG, "Failed to open global output file for writing! ", file.errorString().toLatin1().data());
        return;
    }
    USD_LOG(LOG_DEBUG,"write file:%s",globalFileName(output->hashMd5()).toLatin1().data());
    file.write(QJsonDocument::fromVariant(info).toJson());
    return;
}

bool xrandrOutput::writeGlobalPart(const KScreen::OutputPtr &output, QVariantMap &info,
                             const KScreen::OutputPtr &fallback)
{

    info[QStringLiteral("id")] = output->hash();
    info[QStringLiteral("metadata")] = metadata(output);
    info[QStringLiteral("rotation")] = output->rotation();

    // Round scale to four digits
    info[QStringLiteral("scale")] = int(output->scale() * 10000 + 0.5) / 10000.;
    info[QStringLiteral("dpi")] = UsdBaseClass::getDPI();
    QVariantMap modeInfo;
    float refreshRate = -1.;
    QSize modeSize;
    if (output->currentMode() && output->isEnabled()) {
        refreshRate = output->currentMode()->refreshRate();
        modeSize = output->currentMode()->size();
    }

    if (refreshRate < 0 || !modeSize.isValid()) {
        return false;
    }

    modeInfo[QStringLiteral("refresh")] = refreshRate;

    QVariantMap modeSizeMap;
    modeSizeMap[QStringLiteral("width")] = modeSize.width();
    modeSizeMap[QStringLiteral("height")] = modeSize.height();
    modeInfo[QStringLiteral("size")] = modeSizeMap;

    info[QStringLiteral("mode")] = modeInfo;

    return true;
}
