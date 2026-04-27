#ifndef KF_LOCAL_HALF_H
#define KF_LOCAL_HALF_H

struct half
{
    float value;

    half()
        : value(0.0f)
    {
    }

    half(float v)
        : value(v)
    {
    }

    half(double v)
        : value(static_cast<float>(v))
    {
    }

    half &operator=(float v)
    {
        value = v;
        return *this;
    }

    half &operator=(double v)
    {
        value = static_cast<float>(v);
        return *this;
    }

    operator float() const
    {
        return value;
    }
};

#endif
