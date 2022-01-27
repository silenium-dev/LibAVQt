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
        ~FBOPoolPrivate();

    private:
        FBOPool *q_ptr;

        QSize fboSize{};
        bool dynamic{false};
        size_t maxSize{0};
        size_t initialSize{0};

        QMutex poolMutex{};
        QQueue<uint64_t> fboPool_free{};
        std::map<uint64_t, std::weak_ptr<QOpenGLFramebufferObject>> fboPool_used{};
        std::map<uint32_t, std::unique_ptr<QOpenGLFramebufferObject>> fboPool_all{};

        std::atomic_uint64_t fboPool_idCounter{0};

        QWaitCondition fboPool_available{};

        friend class FBOReturner;
    };

    class FBOReturner {
    public:
        explicit FBOReturner(FBOPoolPrivate *d, uint64_t fboId) : p(d), m_fboId(fboId) {}
        FBOReturner(const FBOReturner &) = default;

        void operator()(QOpenGLFramebufferObject *fbo);

    private:
        bool m_isDestructing{false};
        uint64_t m_fboId;
        FBOPoolPrivate *p;

        friend class FBOPoolPrivate;
    };
}// namespace AVQt::common


#endif//LIBAVQT_FBOPOOLPRIVATE_HPP
