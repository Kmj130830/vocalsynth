/****************************************************************************
** Meta object code from reading C++ file 'MainToolBar.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../include/UI/MainToolBar.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainToolBar.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN7myvocal11MainToolBarE_t {};
} // unnamed namespace

template <> constexpr inline auto myvocal::MainToolBar::qt_create_metaobjectdata<qt_meta_tag_ZN7myvocal11MainToolBarE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "myvocal::MainToolBar",
        "toolChanged",
        "",
        "EditTool",
        "snapToggled",
        "gridToggled"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'toolChanged'
        QtMocHelpers::SignalData<void(EditTool)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 2 },
        }}),
        // Signal 'snapToggled'
        QtMocHelpers::SignalData<void(bool)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 2 },
        }}),
        // Signal 'gridToggled'
        QtMocHelpers::SignalData<void(bool)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 2 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MainToolBar, qt_meta_tag_ZN7myvocal11MainToolBarE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject myvocal::MainToolBar::staticMetaObject = { {
    QMetaObject::SuperData::link<QToolBar::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7myvocal11MainToolBarE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7myvocal11MainToolBarE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN7myvocal11MainToolBarE_t>.metaTypes,
    nullptr
} };

void myvocal::MainToolBar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainToolBar *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->toolChanged((*reinterpret_cast<std::add_pointer_t<EditTool>>(_a[1]))); break;
        case 1: _t->snapToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 2: _t->gridToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (MainToolBar::*)(EditTool )>(_a, &MainToolBar::toolChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (MainToolBar::*)(bool )>(_a, &MainToolBar::snapToggled, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (MainToolBar::*)(bool )>(_a, &MainToolBar::gridToggled, 2))
            return;
    }
}

const QMetaObject *myvocal::MainToolBar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *myvocal::MainToolBar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7myvocal11MainToolBarE_t>.strings))
        return static_cast<void*>(this);
    return QToolBar::qt_metacast(_clname);
}

int myvocal::MainToolBar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QToolBar::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void myvocal::MainToolBar::toolChanged(EditTool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void myvocal::MainToolBar::snapToggled(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void myvocal::MainToolBar::gridToggled(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}
QT_WARNING_POP
