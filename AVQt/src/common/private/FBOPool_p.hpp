//
// Created by silas on 27.01.22.
//

#ifndef LIBAVQT_FBOPOOLPRIVATE_HPP
#define LIBAVQT_FBOPOOLPRIVATE_HPP

#include <QtCore>
#include <QtOpenGL>
#include <queue>
#include <set>

namespace AVQt::common {
    class FBOPool;
    class FBOPoolPrivate {
        Q_DECLARE_PUBLIC(FBOPool)
    public:
        explicit FBOPoolPrivate(FBOPool *q) : q_ptr(q) {}

    private:
        FBOPool *q_ptr;

        QSize fboSize{};
        bool dynamic{false};
        size_t maxSize{0};
        size_t initialSize{0};

        QMutex poolMutex{};
        QQueue<std::shared_ptr<QOpenGLFramebufferObject>> fboPool_free{};
        std::list<std::weak_ptr<QOpenGLFramebufferObject>> fboPool_used{};
        std::list<std::unique_ptr<QOpenGLFramebufferObject>> fboPool_all{};

        QWaitCondition fboPool_available{};
    };
}// namespace AVQt::common


#endif//LIBAVQT_FBOPOOLPRIVATE_HPP
