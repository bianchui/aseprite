namespace doc {
namespace algorithm {

template<typename Traits>
class SrcIter {
public:
    typedef typename Traits::pixel_t pixel_t;
    SrcIter(const Image* src, int y) {
        _src = src;
        _x = 0;
        _y = y;
    }

    SrcIter& operator ++() {
        ++_x;
        return *this;
    }

    pixel_t color() const {
        return get_pixel_fast<Traits>(_src, _x, _y);
    }

private:
    const Image* _src;
    int _x;
    int _y;
};

#define IMG_BITCOUNT 32
#define REVERSE_IMG 0

#if IMG_BITCOUNT == 24
static const uint32_t kBytesPerPixel = 3;
#else//IMG_BITCOUNT == 32
static const uint32_t kBytesPerPixel = 4;
#endif//IMG_BITCOUNT

union MyColor {
    struct { uint8_t r, g, b, a; };
    uint16_t rg;
    uint32_t color;
    uint8_t x[4];

    MyColor() {
        this->color = 0xff000000;
    }

    explicit MyColor(int color) {
        this->color = color;
    }

    explicit MyColor(const uint8_t* p) {
        x[0] = p[0];
        x[1] = p[1];
        x[2] = p[2];
#if IMG_BITCOUNT == 24
        x[3] = 255;
#else//IMG_BITCOUNT == 32
        x[3] = p[3];
#endif//IMG_BITCOUNT
    }

    MyColor(const MyColor& other) {
        this->color = other.color;
    }

    inline MyColor& operator =(const MyColor& other) {
        color = other.color;
        return *this;
    }

    inline MyColor& operator =(uint32_t c) {
        color = c;
        return *this;
    }

    void operator <<(const uint8_t*& p) {
        x[0] = p[0];
        x[1] = p[1];
        x[2] = p[2];
#if IMG_BITCOUNT == 24
        x[3] = 255;
        p += 3;
#else//IMG_BITCOUNT == 32
        x[3] = p[3];
        p += 4;
#endif//IMG_BITCOUNT
    }

    void operator <<(SrcIter<RgbTraits>& src) {
        color = src.color();
        ++src;
    }

    static void copy(uint8_t* dst, const uint8_t* src) {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
#if IMG_BITCOUNT == 24
#else//IMG_BITCOUNT == 32
        dst[3] = src[3];
#endif//IMG_BITCOUNT
    }

    inline bool operator ==(const MyColor& other) const {
        return color == other.color || (a | other.a) == 0;
    }

    inline bool operator !=(const MyColor& other) const {
        return color != other.color && (a | other.a) != 0;
    }

    friend int operator -(MyColor a1, MyColor a2) {
        if (a1 == a2 || (a1.a | a2.a) == 0) {
            return 0;
        } else {
            return abs(a1.g - a2.g) + abs(a1.b - a2.b) + abs(a1.r - a2.r) + abs(a1.a - a2.a);
        }
    }

    void applyTo(uint8_t* p) const {
        p[0] = x[0];
        p[1] = x[1];
        p[2] = x[2];
#if IMG_BITCOUNT == 24
#else//IMG_BITCOUNT == 32
        p[3] = x[3];
#endif//IMG_BITCOUNT
    }
};

static ImageIterator<doc::RgbTraits>& operator <<(ImageIterator<doc::RgbTraits>& it, MyColor color) {
    *it = color.color;
    it += 1;
    return it;
}

template <typename T>
inline T nextLine(T p, uint32_t w) {
#if REVERSE_IMG
    p -= w;
    return p;
#else//REVERSE_IMG
    p += w;
    return p;
#endif//REVERSE_IMG
}

template <typename T>
inline T prevLine(T p, uint32_t w) {
#if REVERSE_IMG
    p += w;
    return p;
#else//REVERSE_IMG
    p -= w;
    return p;
#endif//REVERSE_IMG
}

/**
 * a == b || (a - b <= c - b && a - b <= c - a)
 */
inline bool isSameOrDiffLessThan(MyColor a, MyColor b, MyColor c) {
    return a == b || (a - b <= c - b && a - b <= c - a);
}

static void image_scale2x_rgba(Image* dst, const Image* src, int src_w, int src_h) {
    LockImageBits<RgbTraits> dstBits(dst, gfx::Rect(0, 0, src_w*2, src_h*2));
    auto dstIt = dstBits.begin();

    if (src_w == 0 || src_h == 0) {
        return;
    }

    // c7 c8 c9
    // c4 c5 c6 <- src
    // c1 c2 c3

    MyColor c0(get_pixel_fast<RgbTraits>(src, 0, 0));
    MyColor cT = c0;// (0);// transparent

    MyColor c7, c8, c9;
    MyColor c4, c5, c6;
    MyColor c1, c2, c3;

    {
        auto dst0 = dstIt;
        auto dst1 = nextLine(dstIt, src_w * 2);
        SrcIter<RgbTraits> src0(src, 0);
        SrcIter<RgbTraits> src1(src, src_h > 1 ? 1 : 0);

        // c5 c6
        // c2 c3
        c5 << src0;
        c2 << src1;
        if (src_w > 1) {
            c6 << src0;
            c3 << src1;
        } else {
            c6 = c5;
            c3 = c2;
        }

        if (c6 == c5 || c2 == c5) {
            dst0 << c5 << c5;
            dst1 << c5 << c5;
        } else {
            dst0 << c5 << c5;
            dst1 << c5;

            if (!isSameOrDiffLessThan(c2, c6, c5) ||
                (c3 == c5 && isSameOrDiffLessThan(c2, c0, c5)) ||
                (c3 == c5 && cT == c5 && isSameOrDiffLessThan(c2, cT, c5) && isSameOrDiffLessThan(c6, cT, c5))) {
                dst1 << c5;
            } else {
                dst1 << c2;
            }
        }

        for (int loopX = src_w - 2; loopX > 0; --loopX) {
            // c5 c6 => c4 < c5 < c6 < src0
            // c2 c3 => c1 < c2 < c3 < src1
            c4 = c5; c5 = c6; c6 << src0;
            c1 = c2; c2 = c3; c3 << src1;

            if (isSameOrDiffLessThan(c4, c6, c5) || c2 == c5) {
                dst0 << c5 << c5;
                dst1 << c5 << c5;
            } else {
                dst0 << c5 << c5;

                if (!isSameOrDiffLessThan(c4, c2, c5) ||
                    (c1 == c5 && isSameOrDiffLessThan(c4, c0, c5)) ||
                    (c1 == c5 && cT == c5 && isSameOrDiffLessThan(c4, cT, c5) && isSameOrDiffLessThan(c2, c3, c5))) {
                    dst1 << c5;
                } else {
                    dst1 << c4;
                }

                if (!isSameOrDiffLessThan(c2, c6, c5) ||
                    (c3 == c5 && isSameOrDiffLessThan(c2, c0, c5)) ||
                    (c3 == c5 && cT == c5 && isSameOrDiffLessThan(c2, c1, c5) && isSameOrDiffLessThan(c6, cT, c5))) {
                    dst1 << c5;
                } else {
                    dst1 << c2;
                }
            }
        } // END for (loopX)

        if (src_w > 1) {
            // c5 c6 => c4 < c5 < c6
            // c2 c3 => c1 < c2 < c3
            c4 = c5; c5 = c6;
            c1 = c2; c2 = c3;
            if (c4 == c5 || c2 == c5) {
                dst0 << c5 << c5;
                dst1 << c5 << c5;
            } else {
                dst0 << c5 << c5;

                if (!isSameOrDiffLessThan(c4, c2, c5) ||
                    (c1 == c5 && isSameOrDiffLessThan(c4, c0, c5)) ||
                    (c1 == c5 && cT == c5 && isSameOrDiffLessThan(c4, cT, c5) && isSameOrDiffLessThan(c2, cT, c5))) {
                    dst1 << c5 << c5;
                } else {
                    dst1 << c4 << c5;
                }
            }
        }

        dstIt = dst1;
    }

    for (int loopY = src_h - 2, y = 1; loopY > 0; --loopY, ++y) {
        auto dst0 = dstIt;
        auto dst1 = nextLine(dstIt, src_w * 2);

        SrcIter<RgbTraits> srcUp(src, y - 1);
        SrcIter<RgbTraits> src0(src, y);
        SrcIter<RgbTraits> src1(src, y + 1);
        // srcUp: c8 c9
        // src0:  c5 c6
        // src1:  c2 c3
        c8 << srcUp;
        c5 << src0;
        c2 << src1;
        if (src_w > 1) {
            c9 << srcUp;
            c6 << src0;
            c3 << src1;
        } else {
            c9 = c8;
            c6 = c5;
            c3 = c2;
        }

        if (c6 == c5 || isSameOrDiffLessThan(c2, c8, c5)) {
            dst0 << c5 << c5;
            dst1 << c5 << c5;
        } else {
            dst0 << c5;

            if (!isSameOrDiffLessThan(c6, c8, c5) ||
                (c9 == c5 && isSameOrDiffLessThan(c6, c0, c5)) ||
                (c9 == c5 && cT == c5 && isSameOrDiffLessThan(c6, c3, c5) && isSameOrDiffLessThan(c8, cT, c5))) {
                dst0 << c5;
            } else {
                dst0 << c6;
            }

            dst1 << c5;

            if (!isSameOrDiffLessThan(c2, c6, c5) ||
                (c3 == c5 && isSameOrDiffLessThan(c2, c0, c5)) ||
                (c3 == c5 && cT == c5 && isSameOrDiffLessThan(c2, cT, c5) && isSameOrDiffLessThan(c6, c9, c5))) {
                dst1 << c5;
            } else {
                dst1 << c2;
            }
        }

        for (int loopX = src_w - 2; loopX > 0; --loopX) {
            // c8 c9 => c7 < c8 < c9 < srcUp
            // c5 c6 => c4 < c5 < c6 < src0
            // c2 c3 => c1 < c2 < c3 < src1
            c7 = c8; c8 = c9; c9 << srcUp;
            c4 = c5; c5 = c6; c6 << src0;
            c1 = c2; c2 = c3; c3 << src1;

            if (isSameOrDiffLessThan(c4, c6, c5) ||
                isSameOrDiffLessThan(c2, c8, c5) ||
                ((c4 != c5 && c2 != c5 && c6 != c5 && c8 != c5)
                    && ((isSameOrDiffLessThan(c7, c3, c5) && c1 != c5 && c9 != c5)
                        || (isSameOrDiffLessThan(c1, c9, c5) && c7 != c5 && c3 != c5)))
                ) {
                dst0 << c5 << c5;
                dst1 << c5 << c5;
            } else {
                if (!isSameOrDiffLessThan(c8, c4, c5) ||
                    (c7 == c5 && isSameOrDiffLessThan(c8, c0, c5)) ||
                    (c7 == c5 && c3 == c5 && isSameOrDiffLessThan(c8, c9, c5) && isSameOrDiffLessThan(c4, c1, c5))) {
                    dst0 << c5;
                } else {
                    dst0 << c8;
                }

                if (!isSameOrDiffLessThan(c6, c8, c5) ||
                    (c9 == c5 && isSameOrDiffLessThan(c6, c0, c5)) ||
                    (c9 == c5 && c1 == c5 && isSameOrDiffLessThan(c6, c3, c5) && isSameOrDiffLessThan(c8, c7, c5))) {
                    dst0 << c5;
                } else {
                    dst0 << c6;
                }

                if (!isSameOrDiffLessThan(c4, c2, c5) ||
                    (c1 == c5 && isSameOrDiffLessThan(c4, c0, c5)) ||
                    (c1 == c5 && c9 == c5 && isSameOrDiffLessThan(c4, c7, c5) && isSameOrDiffLessThan(c2, c3, c5))) {
                    dst1 << c5;
                } else {
                    dst1 << c4;
                }

                if (!isSameOrDiffLessThan(c2, c6, c5) ||
                    (c3 == c5 && isSameOrDiffLessThan(c2, c0, c5)) ||
                    (c3 == c5 && c7 == c5 && isSameOrDiffLessThan(c2, c1, c5) && isSameOrDiffLessThan(c6, c9, c5))) {
                    dst1 << c5;
                } else {
                    dst1 << c2;
                }
            }
        } // END for (loopX)

        if (src_w > 1) {
            // c8 c9 => c7 < c8 < c9
            // c5 c6 => c4 < c5 < c6
            // c2 c3 => c1 < c2 < c3
            c7 = c8; c8 = c9;
            c4 = c5; c5 = c6;
            c1 = c2; c2 = c3;

            if (c4 == c5 || isSameOrDiffLessThan(c2, c8, c5)) {
                dst0 << c5 << c5;
                dst1 << c5 << c5;
            } else {
                if (!isSameOrDiffLessThan(c8, c4, c5) ||
                    (c7 == c5 && isSameOrDiffLessThan(c8, c0, c5)) ||
                    (c7 == c5 && cT == c5 && isSameOrDiffLessThan(c8, cT, c5) && isSameOrDiffLessThan(c4, c1, c5))) {
                    dst0 << c5;
                } else {
                    dst0 << c8;
                }
                dst0 << c5;

                if (!isSameOrDiffLessThan(c4, c2, c5) ||
                    (c1 == c5 && isSameOrDiffLessThan(c4, c0, c5)) ||
                    (c1 == c5 && cT == c5 && isSameOrDiffLessThan(c4, c7, c5) && isSameOrDiffLessThan(c2, cT, c5))) {
                    dst1 << c5;
                } else {
                    dst1 << c4;
                }

                dst1 << c5;
            }
        }
        dstIt = dst1;
    } // END for (loopY)

    if (src_h > 1) {// line n - 1
        // srcUp: c8 c9
        // src0:  c5 c6
        SrcIter<RgbTraits> srcUp(src, src_h - 2);
        SrcIter<RgbTraits> src0(src, src_h - 1);

        auto dst0 = dstIt;
        auto dst1 = nextLine(dstIt, src_w * 2);
        c8 << srcUp;
        c5 << src0;
        if (src_w > 1) {
            c9 << srcUp;
            c6 << src0;
        } else {
            c9 = c8;
            c6 = c5;
        }

        if (c6 == c5 || c8 == c5) {
            dst0 << c5 << c5;
        } else {
            dst0 << c5;
            if (!isSameOrDiffLessThan(c6, c8, c5) ||
                (c9 == c5 && isSameOrDiffLessThan(c6, c0, c5)) ||
                (c9 == c5 && cT == c5 && isSameOrDiffLessThan(c6, cT, c5) && isSameOrDiffLessThan(c8, c7, c5))) {
                dst0 << c5;
            } else {
                dst0 << c6;
            }
        }

        dst1 << c5 << c5;

        for (int loopX = src_w - 2; loopX > 0; --loopX) {
            // srcUp: c7 < c8 < c9 < srcUp
            // src0:  c4 < c5 < c6 < src0
            c7 = c8; c8 = c9; c9 << srcUp;
            c4 = c5; c5 = c6; c6 << src0;

            if (isSameOrDiffLessThan(c4, c6, c5) || c8 == c5) {
                dst0 << c5 << c5;
            } else {
                if (!isSameOrDiffLessThan(c8, c4, c5) ||
                    (c7 == c5 && isSameOrDiffLessThan(c8, c0, c5)) ||
                    (c7 == c5 && cT == c5 && isSameOrDiffLessThan(c8, c9, c5) && isSameOrDiffLessThan(c4, cT, c5))) {
                    dst0 << c5;
                } else {
                    dst0 << c8;
                }

                if (!isSameOrDiffLessThan(c6, c8, c5) ||
                    (c9 == c5 && isSameOrDiffLessThan(c6, c0, c5)) ||
                    (c9 == c5 && cT == c5 && isSameOrDiffLessThan(c6, cT, c5) && isSameOrDiffLessThan(c8, c7, c5))) {
                    dst0 << c5;
                } else {
                    dst0 << c6;
                }
            }

            dst1 << c5 << c5;
        } // END for (loopX)

        // srcUp: c7 < c8 < c9
        // src0:  c4 < c5 < c6
        c4 = c5; c5 = c6;
        c7 = c8; c8 = c9;
        if (c6 == c5 || c8 == c5) {
            dst0 << c5 << c5;
            dst1 << c5;
        } else {
            if (!isSameOrDiffLessThan(c8, c4, c5) ||
                (c7 == c5 && isSameOrDiffLessThan(c8, c0, c5)) ||
                (c7 == c5 && cT == c5 && isSameOrDiffLessThan(c8, cT, c5) && isSameOrDiffLessThan(c4, c1, c5))) {
                dst0 << c5;
            } else {
                dst0 << c8;
            }
            dst0 << c5;
        }

        dst1 << c5;
    }
}

} // namespace algorithm
} // namespace doc
