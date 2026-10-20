/****************************************************************************
**
** Copyright (C) 2018 basysKom GmbH, opensource@basyskom.com
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the QtOpcUa module.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "U70Luch_BeamProfileMeter.h"
#include <qopen62541utils.h>
#include <qopen62541valueconverter.h>
#include <QtOpcUa/qopcuatype.h>

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QUuid>

#include <cstring>

QT_BEGIN_NAMESPACE

// Node ID conversion is included from the open62541 plugin but warnings from there should be logged
// using qt.opcua.testserver instead of qt.opcua.plugins.open62541 for usage in the test server
Q_LOGGING_CATEGORY(QT_OPCUA_PLUGINS_OPEN62541, "qt.opcua.u70luch_beamprofilemeter_server")

BeamProfileMeterServer::BeamProfileMeterServer(QObject *parent)
  : QObject(parent)
  , m_state(BeamProfileMeterServer::MachineState::Idle)
  , m_percentFilledTank1(100)
  , m_percentFilledTank2(0)
{
  m_timer.setInterval(0);
  m_timer.setSingleShot(false);
  m_machineTimer.setInterval(200);
  connect(&m_timer, &QTimer::timeout, this, &BeamProfileMeterServer::processServerEvents);
}

BeamProfileMeterServer::~BeamProfileMeterServer()
{
  shutdown();
  UA_Server_delete(m_server);
  UA_NodeId_deleteMembers(&m_percentFilledTank1Node);
  UA_NodeId_deleteMembers(&m_percentFilledTank2Node);
  UA_NodeId_deleteMembers(&m_tank2TargetPercentNode);
  UA_NodeId_deleteMembers(&m_tank2ValveStateNode);
  UA_NodeId_deleteMembers(&m_machineStateNode);
}

bool BeamProfileMeterServer::init()
{
  m_server = UA_Server_new();

  if (!m_server)
  {
    return false;
  }
  UA_StatusCode result = UA_ServerConfig_setMinimal(UA_Server_getConfig(m_server), 43344, nullptr);

  if (result != UA_STATUSCODE_GOOD)
  {
    return false;
  }
  return true;
}

void BeamProfileMeterServer::processServerEvents()
{
  if (m_running)
  {
    UA_Server_run_iterate(m_server, true);
  }
}

void BeamProfileMeterServer::shutdown()
{
  if (m_running)
  {
    UA_Server_run_shutdown(m_server);
    m_running = false;
  }
}

UA_NodeId BeamProfileMeterServer::addObject(const QString &parent, const QString &nodeString, const QString &browseName,
  const QString &displayName, const QString &description, quint32 referenceType)
{
  UA_NodeId resultNode;
  UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;

  oAttr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", displayName.toUtf8().constData());
  if (description.size())
  {
    oAttr.description = UA_LOCALIZEDTEXT_ALLOC("en-US", description.toUtf8().constData());
  }
  UA_StatusCode result;
  UA_NodeId requestedNodeId = Open62541Utils::nodeIdFromQString(nodeString);
  UA_NodeId parentNodeId = Open62541Utils::nodeIdFromQString(parent);

  UA_QualifiedName nodeBrowseName = UA_QUALIFIEDNAME_ALLOC(requestedNodeId.namespaceIndex, browseName.toUtf8().constData());

  result = UA_Server_addObjectNode(m_server,
    requestedNodeId,
    parentNodeId,
    UA_NODEID_NUMERIC(0, referenceType),
    nodeBrowseName,
    UA_NODEID_NULL,
    oAttr,
    nullptr,
    &resultNode);

  UA_QualifiedName_deleteMembers(&nodeBrowseName);
  UA_NodeId_deleteMembers(&requestedNodeId);
  UA_NodeId_deleteMembers(&parentNodeId);
  UA_ObjectAttributes_deleteMembers(&oAttr);

  if (result != UA_STATUSCODE_GOOD)
  {
    qWarning() << "Could not add folder:" << nodeString << " :" << result;
    return UA_NODEID_NULL;
  }
  return resultNode;
}

UA_NodeId BeamProfileMeterServer::addVariable(const UA_NodeId &folder, const QString &variableNode, const QString &browseName,
  const QString &displayName, const QVariant &value, QOpcUa::Types type,
    quint32 referenceType)
{
  UA_NodeId variableNodeId = Open62541Utils::nodeIdFromQString(variableNode);

  UA_VariableAttributes attr = UA_VariableAttributes_default;
  attr.value = QOpen62541ValueConverter::toOpen62541Variant(value, type);
  attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", displayName.toUtf8().constData());
  attr.dataType = attr.value.type ? attr.value.type->typeId : UA_TYPES[UA_TYPES_BOOLEAN].typeId;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

  UA_QualifiedName variableName = UA_QUALIFIEDNAME_ALLOC(variableNodeId.namespaceIndex, browseName.toUtf8().constData());

  UA_NodeId resultId;
  UA_StatusCode result = UA_Server_addVariableNode(m_server,
    variableNodeId,
    folder,
    UA_NODEID_NUMERIC(0, referenceType),
    variableName,
    UA_NODEID_NULL,
    attr,
    nullptr,
    &resultId);

//  UA_NodeId_deleteMembers(&variableNodeId);
//  UA_VariableAttributes_deleteMembers(&attr);
//  UA_QualifiedName_deleteMembers(&variableName);

  if (result != UA_STATUSCODE_GOOD)
  {
    qWarning() << "Could not add variable:" << result;
    return UA_NODEID_NULL;
  }

  return resultId;
}

UA_NodeId BeamProfileMeterServer::addVariableArray(const UA_NodeId &folder, const QString &variableNode, const QString &browseName,
  const QString &displayName, const QVariantList &value, QOpcUa::Types type,
    quint32 referenceType)
{
  UA_NodeId variableNodeId = Open62541Utils::nodeIdFromQString(variableNode);

  UA_VariableAttributes attr = UA_VariableAttributes_default;
  attr.value = QOpen62541ValueConverter::toOpen62541Variant(value, type);
  attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", displayName.toUtf8().constData());
  attr.dataType = attr.value.type ? attr.value.type->typeId : UA_TYPES[UA_TYPES_BOOLEAN].typeId;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
  attr.valueRank = UA_VALUERANK_ONE_DIMENSION;
  attr.arrayDimensionsSize = 1;
  UA_UInt32 dimSize = value.size();
  attr.arrayDimensions = &dimSize;


  UA_QualifiedName variableName = UA_QUALIFIEDNAME_ALLOC(variableNodeId.namespaceIndex, browseName.toUtf8().constData());

  UA_NodeId resultId;
  UA_StatusCode result = UA_Server_addVariableNode(m_server,
    variableNodeId,
    folder,
    UA_NODEID_NUMERIC(0, referenceType),
    variableName,
    UA_NODEID_NULL,
    attr,
    nullptr,
    &resultId);

//  UA_NodeId_deleteMembers(&variableNodeId);
//  UA_VariableAttributes_deleteMembers(&attr);
//  UA_QualifiedName_deleteMembers(&variableName);

  if (result != UA_STATUSCODE_GOOD)
  {
    qWarning() << "Could not add variable:" << result;
    return UA_NODEID_NULL;
  }

  return resultId;
}

UA_StatusCode BeamProfileMeterServer::startPumpMethod(UA_Server *server, const UA_NodeId *sessionId, void *sessionHandle, const UA_NodeId *methodId, void *methodContext, const UA_NodeId *objectId, void *objectContext, size_t inputSize, const UA_Variant *input, size_t outputSize, UA_Variant *output)
{
    Q_UNUSED(server);
    Q_UNUSED(sessionId);
    Q_UNUSED(sessionHandle);
    Q_UNUSED(methodId);
    Q_UNUSED(objectId);
    Q_UNUSED(objectContext);
    Q_UNUSED(inputSize);
    Q_UNUSED(input);
    Q_UNUSED(outputSize);
    Q_UNUSED(output);

    BeamProfileMeterServer *data = static_cast<BeamProfileMeterServer *>(methodContext);

    double targetValue = data->readTank2TargetValue();

    if (data->m_state == MachineState::Idle && data->m_percentFilledTank1 > 0 && data->m_percentFilledTank2 < targetValue) {
        qDebug() << "Start pumping";
        data->setState(MachineState::Pumping);
        data->m_machineTimer.start();
        return UA_STATUSCODE_GOOD;
    }
    else {
        qDebug() << "Machine already running";
        return UA_STATUSCODE_BADUSERACCESSDENIED;
    }
}

UA_StatusCode BeamProfileMeterServer::stopPumpMethod(UA_Server *server, const UA_NodeId *sessionId, void *sessionHandle, const UA_NodeId *methodId, void *methodContext, const UA_NodeId *objectId, void *objectContext, size_t inputSize, const UA_Variant *input, size_t outputSize, UA_Variant *output)
{
    Q_UNUSED(server);
    Q_UNUSED(sessionId);
    Q_UNUSED(sessionHandle);
    Q_UNUSED(methodId);
    Q_UNUSED(objectId);
    Q_UNUSED(objectContext);
    Q_UNUSED(inputSize);
    Q_UNUSED(input);
    Q_UNUSED(outputSize);
    Q_UNUSED(output);

    BeamProfileMeterServer *data = static_cast<BeamProfileMeterServer *>(methodContext);

    if (data->m_state == MachineState::Pumping) {
        qDebug() << "Stopping";
        data->m_machineTimer.stop();
        data->setState(MachineState::Idle);
        return UA_STATUSCODE_GOOD;
    } else {
        qDebug() << "Nothing to stop";
        return UA_STATUSCODE_BADUSERACCESSDENIED;
    }
}

UA_StatusCode BeamProfileMeterServer::flushTank2Method(UA_Server *server, const UA_NodeId *sessionId, void *sessionHandle, const UA_NodeId *methodId, void *methodContext, const UA_NodeId *objectId, void *objectContext, size_t inputSize, const UA_Variant *input, size_t outputSize, UA_Variant *output)
{
    Q_UNUSED(server);
    Q_UNUSED(sessionId);
    Q_UNUSED(sessionHandle);
    Q_UNUSED(methodId);
    Q_UNUSED(objectId);
    Q_UNUSED(objectContext);
    Q_UNUSED(inputSize);
    Q_UNUSED(input);
    Q_UNUSED(outputSize);
    Q_UNUSED(output);

    BeamProfileMeterServer *data = static_cast<BeamProfileMeterServer *>(methodContext);

    double targetValue = data->readTank2TargetValue();

    if (data->m_state == MachineState::Idle && data->m_percentFilledTank2 > targetValue) {
        data->setState(MachineState::Flushing);
        qDebug() << "Flushing tank 2";
        data->setTank2ValveState(true);
        data->m_machineTimer.start();
        return UA_STATUSCODE_GOOD;
    }
    else {
        qDebug() << "Unable to comply";
        return UA_STATUSCODE_BADUSERACCESSDENIED;
    }
}

UA_StatusCode BeamProfileMeterServer::resetMethod(UA_Server *server, const UA_NodeId *sessionId, void *sessionHandle, const UA_NodeId *methodId, void *methodContext, const UA_NodeId *objectId, void *objectContext, size_t inputSize, const UA_Variant *input, size_t outputSize, UA_Variant *output)
{
    Q_UNUSED(server);
    Q_UNUSED(sessionId);
    Q_UNUSED(sessionHandle);
    Q_UNUSED(methodId);
    Q_UNUSED(objectId);
    Q_UNUSED(objectContext);
    Q_UNUSED(inputSize);
    Q_UNUSED(input);
    Q_UNUSED(outputSize);
    Q_UNUSED(output);

    BeamProfileMeterServer *data = static_cast<BeamProfileMeterServer *>(methodContext);

        qDebug() << "Reset simulation";
        data->setState(MachineState::Idle);
        data->m_machineTimer.stop();
        data->setTank2ValveState(false);
        data->setPercentFillTank1(100);
        data->setPercentFillTank2(0);
        return UA_STATUSCODE_GOOD;
}

void BeamProfileMeterServer::setState(BeamProfileMeterServer::MachineState state)
{
    UA_Variant val;
    m_state = state;
    UA_Variant_setScalarCopy(&val, &state, &UA_TYPES[UA_TYPES_UINT32]);
    UA_Server_writeValue(m_server, m_machineStateNode, val);
}

void BeamProfileMeterServer::setPercentFillTank1(double fill)
{
    UA_Variant val;
    m_percentFilledTank1 = fill;
    UA_Variant_setScalarCopy(&val, &fill, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(this->m_server, this->m_percentFilledTank1Node, val);
}

void BeamProfileMeterServer::setPercentFillTank2(double fill)
{
    UA_Variant val;
    m_percentFilledTank2 = fill;
    UA_Variant_setScalarCopy(&val, &fill, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(this->m_server, this->m_percentFilledTank2Node, val);
}

void BeamProfileMeterServer::setTank2ValveState(bool state)
{
    UA_Variant val;
    UA_Variant_setScalarCopy(&val, &state, &UA_TYPES[UA_TYPES_BOOLEAN]);
    UA_Server_writeValue(this->m_server, this->m_tank2ValveStateNode, val);
}

double BeamProfileMeterServer::readTank2TargetValue()
{
    UA_Variant var;
    UA_Server_readValue(m_server, m_tank2TargetPercentNode, &var);
    return static_cast<double *>(var.data)[0];
}

UA_NodeId BeamProfileMeterServer::addMethod(const UA_NodeId &folder, const QString &variableNode, const QString &description,
                                const QString &browseName, const QString &displayName, UA_MethodCallback cb,
                                quint32 referenceType)
{
    UA_NodeId methodNodeId = Open62541Utils::nodeIdFromQString(variableNode);

    UA_MethodAttributes attr = UA_MethodAttributes_default;

    attr.description = UA_LOCALIZEDTEXT_ALLOC("en-US", description.toUtf8().constData());
    attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", displayName.toUtf8().constData());
    attr.executable = true;
    UA_QualifiedName methodBrowseName = UA_QUALIFIEDNAME_ALLOC(methodNodeId.namespaceIndex, browseName.toUtf8().constData());

    UA_NodeId resultId;
    UA_StatusCode result = UA_Server_addMethodNode(m_server, methodNodeId, folder,
                                                     UA_NODEID_NUMERIC(0, referenceType),
                                                     methodBrowseName,
                                                     attr, cb,
                                                     0, nullptr,
                                                     0, nullptr,
                                                     this, &resultId);

    UA_NodeId_deleteMembers(&methodNodeId);
    UA_MethodAttributes_deleteMembers(&attr);
    UA_QualifiedName_deleteMembers(&methodBrowseName);

    if (result != UA_STATUSCODE_GOOD) {
        qWarning() << "Could not add Method:" << result;
        return UA_NODEID_NULL;
    }
    return resultId;
}

void BeamProfileMeterServer::launch()
{
  UA_StatusCode s = UA_Server_run_startup(m_server);
  if (s != UA_STATUSCODE_GOOD)
  {
    qFatal("Could not launch server");
  }
  m_running = true;
  m_timer.start();

  int ns1 = UA_Server_addNamespace(m_server, "U70Luch");
  if (ns1 != 2)
  {
    qFatal("Unexpected namespace index for U70Luch");
  }
  UA_NodeId u70LuchObject = this->addObject(QOpcUa::namespace0Id(QOpcUa::NodeIds::Namespace0::ObjectsFolder), "ns=2;s=U70Luch",
    "U70Luch", "U70Luch", "Beam profile meters information", UA_NS0ID_ORGANIZES);
  UA_NodeId room1LuchObject = this->addObject("ns=2;s=U70Luch", "ns=2;s=U70Luch.Room1", "Room1", "Treatment room 1");
  UA_NodeId room2LuchObject = this->addObject("ns=2;s=U70Luch", "ns=2;s=U70Luch.Room2", "Room2", "Treatment room 2");
  UA_NodeId room3LuchObject = this->addObject("ns=2;s=U70Luch", "ns=2;s=U70Luch.Room3", "Room3", "Treatment room 3");

  UA_NodeId isoRoom1LuchObject = this->addObject("ns=2;s=U70Luch.Room1", "ns=2;s=U70Luch.Room1.ProfilesIso", "ProfilesIso", "Isocemter beam profile information");
  UA_NodeId isoRoom2LuchObject = this->addObject("ns=2;s=U70Luch.Room2", "ns=2;s=U70Luch.Room2.ProfilesIso", "ProfilesIso", "Isocemter beam profile information");
  UA_NodeId isoRoom3LuchObject = this->addObject("ns=2;s=U70Luch.Room3", "ns=2;s=U70Luch.Room3.ProfilesIso", "ProfilesIso", "Isocemter beam profile information");

  m_Room1ProfileIsoIDNode = this->addVariable(isoRoom1LuchObject, "ns=2;s=U70Luch.Room1.ProfilesIso.ID", "ID", "ID", "CHAN26IC01", QOpcUa::Types::String);
  m_Room2ProfileIsoIDNode = this->addVariable(isoRoom2LuchObject, "ns=2;s=U70Luch.Room2.ProfilesIso.ID", "ID", "ID", "CHAN26IC03", QOpcUa::Types::String);
  m_Room3ProfileIsoIDNode = this->addVariable(isoRoom3LuchObject, "ns=2;s=U70Luch.Room3.ProfilesIso.ID", "ID", "ID", "CHAN26IC05", QOpcUa::Types::String);

  QVariantList vl;
  vl.append(QVariant(-1.));
  vl.append(QVariant(0.));
  vl.append(QVariant(5.));
  vl.append(QVariant(32.1));
  vl.append(QVariant(120.));

  m_Room1ProfileIsoSettingsNode = this->addVariableArray(isoRoom1LuchObject, "ns=2;s=U70Luch.Room1.ProfilesIso.Settings", "Settings", "Settings", vl, QOpcUa::Types::Double);
  m_Room2ProfileIsoSettingsNode = this->addVariableArray(isoRoom2LuchObject, "ns=2;s=U70Luch.Room2.ProfilesIso.Settings", "Settings", "Settings", vl, QOpcUa::Types::Double);
  m_Room3ProfileIsoSettingsNode = this->addVariableArray(isoRoom3LuchObject, "ns=2;s=U70Luch.Room3.ProfilesIso.Settings", "Settings", "Settings", vl, QOpcUa::Types::Double);

  QVariantList vl1;
  for(int i = 0; i < 192; ++i)
  {
    vl1.append(QVariant(double(i)));
    vl1.append(QVariant(0.));
  }
  m_Room1ProfileIsoProfileXNode = this->addVariableArray(isoRoom1LuchObject, "ns=2;s=U70Luch.Room1.ProfilesIso.ProfileX", "ProfileX", "ProfileX", vl1, QOpcUa::Types::Double);
  m_Room2ProfileIsoProfileXNode = this->addVariableArray(isoRoom2LuchObject, "ns=2;s=U70Luch.Room2.ProfilesIso.ProfileX", "ProfileX", "ProfileX", vl1, QOpcUa::Types::Double);
  m_Room3ProfileIsoProfileXNode = this->addVariableArray(isoRoom3LuchObject, "ns=2;s=U70Luch.Room3.ProfilesIso.ProfileX", "ProfileX", "ProfileX", vl1, QOpcUa::Types::Double);

  QVariantList vl2;
  for(int i = 0; i < 192; ++i)
  {
    vl2.append(QVariant(double(i)));
    vl2.append(QVariant(1.));
  }

  m_Room1ProfileIsoProfileYNode = this->addVariableArray(isoRoom1LuchObject, "ns=2;s=U70Luch.Room1.ProfilesIso.ProfileY", "ProfileY", "ProfileY", vl2, QOpcUa::Types::Double);
  m_Room2ProfileIsoProfileYNode = this->addVariableArray(isoRoom2LuchObject, "ns=2;s=U70Luch.Room2.ProfilesIso.ProfileY", "ProfileY", "ProfileY", vl2, QOpcUa::Types::Double);
  m_Room3ProfileIsoProfileYNode = this->addVariableArray(isoRoom3LuchObject, "ns=2;s=U70Luch.Room3.ProfilesIso.ProfileY", "ProfileY", "ProfileY", vl2, QOpcUa::Types::Double);

  m_Room1ProfileIsoTimeStepNode = this->addVariable(isoRoom1LuchObject, "ns=2;s=U70Luch.Room1.ProfilesIso.TimeStep", "TimeStep", "TimeStep", 20., QOpcUa::Types::Double);
  m_Room2ProfileIsoTimeStepNode = this->addVariable(isoRoom2LuchObject, "ns=2;s=U70Luch.Room2.ProfilesIso.TimeStep", "TimeStep", "TimeStep", 20., QOpcUa::Types::Double);
  m_Room3ProfileIsoTimeStepNode = this->addVariable(isoRoom3LuchObject, "ns=2;s=U70Luch.Room3.ProfilesIso.TimeStep", "TimeStep", "TimeStep", 20., QOpcUa::Types::Double);

  m_Room1ProfileIsoTableRowsNode = this->addVariable(isoRoom1LuchObject, "ns=2;s=U70Luch.Room1.ProfilesIso.TableRows", "TableRows", "TableRows", 5, QOpcUa::Types::UInt16);
  m_Room2ProfileIsoTableRowsNode = this->addVariable(isoRoom2LuchObject, "ns=2;s=U70Luch.Room2.ProfilesIso.TableRows", "TableRows", "TableRows", 5, QOpcUa::Types::UInt16);
  m_Room3ProfileIsoTableRowsNode = this->addVariable(isoRoom3LuchObject, "ns=2;s=U70Luch.Room3.ProfilesIso.TableRows", "TableRows", "TableRows", 5, QOpcUa::Types::UInt16);

  m_Room1ProfileIsoTableColumnsNode = this->addVariable(isoRoom1LuchObject, "ns=2;s=U70Luch.Room1.ProfilesIso.TableColumns", "TableColumns", "TableColumns", 45, QOpcUa::Types::UInt16);
  m_Room2ProfileIsoTableColumnsNode = this->addVariable(isoRoom2LuchObject, "ns=2;s=U70Luch.Room2.ProfilesIso.TableColumns", "TableColumns", "TableColumns", 45, QOpcUa::Types::UInt16);
  m_Room3ProfileIsoTableColumnsNode = this->addVariable(isoRoom3LuchObject, "ns=2;s=U70Luch.Room3.ProfilesIso.TableColumns", "TableColumns", "TableColumns", 45, QOpcUa::Types::UInt16);

  QVariantList vl3;
  for(int i = 0; i < 4*45; ++i)
  {
    vl3.append(QVariant(double(i)));
  }
  m_Room1ProfileIsoTableNode = this->addVariableArray(isoRoom1LuchObject, "ns=2;s=U70Luch.Room1.ProfilesIso.Table", "Table", "Table", vl3, QOpcUa::Types::Double);
  m_Room2ProfileIsoTableNode = this->addVariableArray(isoRoom2LuchObject, "ns=2;s=U70Luch.Room2.ProfilesIso.Table", "Table", "Table", vl3, QOpcUa::Types::Double);
  m_Room3ProfileIsoTableNode = this->addVariableArray(isoRoom3LuchObject, "ns=2;s=U70Luch.Room3.ProfilesIso.Table", "Table", "Table", vl3, QOpcUa::Types::Double);

/*
  UA_Variant id;
  const char* id1 = "CHAN26IC01";
  const char* id2 = "CHAN26IC03";
  const char* id3 = "CHAN26IC05";
  UA_Variant_setScalarCopy(&id, id1, &UA_TYPES[UA_TYPES_STRING]);
  UA_Server_writeValue(m_server, m_Room1ProfileIsoIDNode, id);

  UA_Variant_setScalarCopy(&id, id2, &UA_TYPES[UA_TYPES_STRING]);
  UA_Server_writeValue(m_server, m_Room2ProfileIsoIDNode, id);

  UA_Variant_setScalarCopy(&id, id3, &UA_TYPES[UA_TYPES_STRING]);
  UA_Server_writeValue(m_server, m_Room3ProfileIsoIDNode, id);
*/
/*
  double set[6] = { -1., 0., 3., 15., 32., 120.};
  UA_Variant settings;
  UA_Variant_setArrayCopy(&settings, set, 6, &UA_TYPES[UA_TYPES_DOUBLE]);
  UA_Server_writeValue(m_server, m_Room1ProfileIsoSettingsNode, settings);
  UA_Server_writeValue(m_server, m_Room2ProfileIsoSettingsNode, settings);
  UA_Server_writeValue(m_server, m_Room3ProfileIsoSettingsNode, settings);

  UA_Variant profile;
  double x[192] = {};
  UA_Variant_setArrayCopy(&profile, x, 192, &UA_TYPES[UA_TYPES_DOUBLE]);
  UA_Server_writeValue(m_server, m_Room1ProfileIsoProfileXNode, profile);
  UA_Server_writeValue(m_server, m_Room1ProfileIsoProfileYNode, profile);
  UA_Server_writeValue(m_server, m_Room2ProfileIsoProfileXNode, profile);
  UA_Server_writeValue(m_server, m_Room2ProfileIsoProfileYNode, profile);
  UA_Server_writeValue(m_server, m_Room3ProfileIsoProfileXNode, profile);
  UA_Server_writeValue(m_server, m_Room3ProfileIsoProfileYNode, profile);

  UA_Variant timeStep;
  double ts = 20.;
  UA_Variant_setScalarCopy(&timeStep, &ts, &UA_TYPES[UA_TYPES_DOUBLE]);
  UA_Server_writeValue(m_server, m_Room1ProfileIsoTimeStepNode, timeStep);
  UA_Server_writeValue(m_server, m_Room2ProfileIsoTimeStepNode, timeStep);
  UA_Server_writeValue(m_server, m_Room3ProfileIsoTimeStepNode, timeStep);

  uint16_t r = 10, c = 10;
  UA_Variant vr, vc;
  UA_Variant_setScalarCopy(&vr, &r, &UA_TYPES[UA_TYPES_UINT16]);
  UA_Variant_setScalarCopy(&vc, &c, &UA_TYPES[UA_TYPES_UINT16]);

  UA_Server_writeValue(m_server, m_Room1ProfileIsoTableColumnsNode, vc);
  UA_Server_writeValue(m_server, m_Room1ProfileIsoTableRowsNode, vr);
  UA_Server_writeValue(m_server, m_Room2ProfileIsoTableColumnsNode, vc);
  UA_Server_writeValue(m_server, m_Room2ProfileIsoTableRowsNode, vr);
  UA_Server_writeValue(m_server, m_Room3ProfileIsoTableColumnsNode, vc);
  UA_Server_writeValue(m_server, m_Room3ProfileIsoTableRowsNode, vr);

  double t[100] = {};
  UA_Variant vt;
  UA_Variant_setArrayCopy(&vt, t, 100, &UA_TYPES[UA_TYPES_DOUBLE]);
  UA_Server_writeValue(m_server, m_Room1ProfileIsoTableNode, vt);
  UA_Server_writeValue(m_server, m_Room2ProfileIsoTableNode, vt);
  UA_Server_writeValue(m_server, m_Room3ProfileIsoTableNode, vt);
*/

  UA_NodeId_deleteMembers(&u70LuchObject);
  UA_NodeId_deleteMembers(&room1LuchObject);
  UA_NodeId_deleteMembers(&room2LuchObject);
  UA_NodeId_deleteMembers(&room3LuchObject);

  UA_NodeId_deleteMembers(&isoRoom1LuchObject);
  UA_NodeId_deleteMembers(&isoRoom2LuchObject);
  UA_NodeId_deleteMembers(&isoRoom3LuchObject);

  UA_NodeId_deleteMembers(&m_Room1ProfileIsoIDNode);
  UA_NodeId_deleteMembers(&m_Room1ProfileIsoSettingsNode);
  UA_NodeId_deleteMembers(&m_Room1ProfileIsoProfileXNode);
  UA_NodeId_deleteMembers(&m_Room1ProfileIsoProfileYNode);
  UA_NodeId_deleteMembers(&m_Room1ProfileIsoTimeStepNode);
  UA_NodeId_deleteMembers(&m_Room1ProfileIsoTableRowsNode);
  UA_NodeId_deleteMembers(&m_Room1ProfileIsoTableColumnsNode);
  UA_NodeId_deleteMembers(&m_Room1ProfileIsoTableNode);

  UA_NodeId_deleteMembers(&m_Room2ProfileIsoIDNode);
  UA_NodeId_deleteMembers(&m_Room2ProfileIsoSettingsNode);
  UA_NodeId_deleteMembers(&m_Room2ProfileIsoProfileXNode);
  UA_NodeId_deleteMembers(&m_Room2ProfileIsoProfileYNode);
  UA_NodeId_deleteMembers(&m_Room2ProfileIsoTimeStepNode);
  UA_NodeId_deleteMembers(&m_Room2ProfileIsoTableRowsNode);
  UA_NodeId_deleteMembers(&m_Room2ProfileIsoTableColumnsNode);
  UA_NodeId_deleteMembers(&m_Room2ProfileIsoTableNode);

  UA_NodeId_deleteMembers(&m_Room3ProfileIsoIDNode);
  UA_NodeId_deleteMembers(&m_Room3ProfileIsoSettingsNode);
  UA_NodeId_deleteMembers(&m_Room3ProfileIsoProfileXNode);
  UA_NodeId_deleteMembers(&m_Room3ProfileIsoProfileYNode);
  UA_NodeId_deleteMembers(&m_Room3ProfileIsoTimeStepNode);
  UA_NodeId_deleteMembers(&m_Room3ProfileIsoTableRowsNode);
  UA_NodeId_deleteMembers(&m_Room3ProfileIsoTableColumnsNode);
  UA_NodeId_deleteMembers(&m_Room3ProfileIsoTableNode);
/*
     UA_NodeId machineObject = addObject(QOpcUa::namespace0Id(QOpcUa::NodeIds::Namespace0::ObjectsFolder), "ns=2;s=Machine",
                                         "Machine", "Machine", "The machine simulator", UA_NS0ID_ORGANIZES);
     UA_NodeId tank1Object = addObject("ns=2;s=Machine", "ns=2;s=Machine.Tank1", "Tank1", "Tank 1");
     UA_NodeId tank2Object = addObject("ns=2;s=Machine", "ns=2;s=Machine.Tank2", "Tank2", "Tank 2");

     m_percentFilledTank1Node = addVariable(tank1Object, "ns=2;s=Machine.Tank1.PercentFilled", "PercentFilled", "Tank 1 Fill Level", 100.0, QOpcUa::Types::Double);
     m_percentFilledTank2Node = addVariable(tank2Object, "ns=2;s=Machine.Tank2.PercentFilled", "PercentFilled", "Tank 2 Fill Level", 0.0, QOpcUa::Types::Double);
     m_tank2TargetPercentNode = addVariable(tank2Object, "ns=2;s=Machine.Tank2.TargetPercent", "TargetPercent", "Tank 2 Target Level", 0.0, QOpcUa::Types::Double);
     m_tank2ValveStateNode = addVariable(tank2Object, "ns=2;s=Machine.Tank2.ValveState", "ValveState", "Tank 2 Valve State", false, QOpcUa::Types::Boolean);
     m_machineStateNode = addVariable(machineObject, "ns=2;s=Machine.State", "State", "Machine State", static_cast<quint32>(MachineState::Idle), QOpcUa::Types::UInt32);
     UA_NodeId tempId;
     tempId = addVariable(machineObject, "ns=2;s=Machine.Designation", "Designation", "Machine Designation", "TankExample", QOpcUa::Types::String);
     UA_NodeId_deleteMembers(&tempId);

     tempId = addMethod(machineObject, "ns=2;s=Machine.Start", "Starts the pump", "Start", "Start Pump", &startPumpMethod);
     UA_NodeId_deleteMembers(&tempId);
     tempId = addMethod(machineObject, "ns=2;s=Machine.Stop", "Stops the pump", "Stop", "Stop Pump", &stopPumpMethod);
     UA_NodeId_deleteMembers(&tempId);
     tempId = addMethod(machineObject, "ns=2;s=Machine.FlushTank2", "Flushes tank 2", "FlushTank2", "Flush Tank 2", &flushTank2Method);
     UA_NodeId_deleteMembers(&tempId);
     tempId = addMethod(machineObject, "ns=2;s=Machine.Reset", "Resets the simulation", "Reset", "Reset Simulation", &resetMethod);
     UA_NodeId_deleteMembers(&tempId);

     UA_NodeId_deleteMembers(&machineObject);
     UA_NodeId_deleteMembers(&tank1Object);
     UA_NodeId_deleteMembers(&tank2Object);

     QObject::connect(&m_machineTimer, &QTimer::timeout, [this]() {

         double targetValue = readTank2TargetValue();
         if (m_state == MachineState::Pumping && m_percentFilledTank1 > 0 && m_percentFilledTank2 < targetValue) {
            setPercentFillTank1(m_percentFilledTank1 - 1);
            setPercentFillTank2(m_percentFilledTank2 + 1);
            if (qFuzzyIsNull(m_percentFilledTank1) || m_percentFilledTank2 >= targetValue) {
                setState(MachineState::Idle);
                m_machineTimer.stop();
            }
         } else if (m_state == MachineState::Flushing && m_percentFilledTank2 > targetValue) {
             setPercentFillTank2(m_percentFilledTank2 - 1);
             if (m_percentFilledTank2 <= targetValue) {
                 setTank2ValveState(false);
                 setState(MachineState::Idle);
                 m_machineTimer.stop();
             }
         }
     });
*/
}

QT_END_NAMESPACE
