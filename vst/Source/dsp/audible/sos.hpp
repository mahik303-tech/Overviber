// Cascaded second-order sections IIR filter
// Copyright (C) 2020 Tyler Coy
// GNU General Public License v3 or later

#pragma once

namespace audible
{

struct SOSCoefficients
{
    float b[3];
    float a[2];
};

template <typename T, int max_num_sections>
class SOSFilter
{
public:
    SOSFilter()
    {
        Init(0);
    }

    SOSFilter(int num_sections)
    {
        Init(num_sections);
    }

    void Init(int num_sections)
    {
        num_sections_ = num_sections;
        Reset();
    }

    void Init(int num_sections, const SOSCoefficients* sections)
    {
        num_sections_ = num_sections;
        Reset();
        SetCoefficients(sections);
    }

    void Reset()
    {
        for (int n = 0; n <= num_sections_; n++)
        {
            x_[n][0] = 0.f;
            x_[n][1] = 0.f;
            x_[n][2] = 0.f;
        }
    }

    // Overviber: coefficients are kept as vectors, so they are not broadcast
    // again for every multiplication.
    void SetCoefficients(const SOSCoefficients* sections)
    {
        for (int n = 0; n < num_sections_; n++)
        {
            b0_[n] = sections[n].b[0];
            b1_[n] = sections[n].b[1];
            b2_[n] = sections[n].b[2];
            a0_[n] = sections[n].a[0];
            a1_[n] = sections[n].a[1];
        }
    }

    // Overviber: dispatches to a cascade with a compile-time section count,
    // which the compiler unrolls. The arithmetic and its order are unchanged.
    T Process(T in)
    {
        switch (num_sections_)
        {
            case 1: return ProcessSections<1>(in);
            case 2: return ProcessSections<2>(in);
            case 3: return ProcessSections<3>(in);
            case 4: return ProcessSections<4>(in);
            case 5: return ProcessSections<5>(in);
            case 6: return ProcessSections<6>(in);
            case 7: return ProcessSections<7>(in);
            case 8: return ProcessSections<8>(in);
            default: return ProcessSections<0>(in);
        }
    }

protected:
    template <int kSections>
    T ProcessSections(T in)
    {
        if constexpr (kSections > max_num_sections)
        {
            return in;   // not reachable: Init() never sets more sections
        }
        else
        {
            for (int n = 0; n < kSections; n++)
            {
                // Shift x state
                x_[n][2] = x_[n][1];
                x_[n][1] = x_[n][0];
                x_[n][0] = in;

                T out = 0.f;

                // Add x state
                out += b0_[n] * x_[n][0];
                out += b1_[n] * x_[n][1];
                out += b2_[n] * x_[n][2];

                // Subtract y state
                out -= a0_[n] * x_[n+1][0];
                out -= a1_[n] * x_[n+1][1];
                in = out;
            }

            // Shift final section x state
            x_[kSections][2] = x_[kSections][1];
            x_[kSections][1] = x_[kSections][0];
            x_[kSections][0] = in;

            return in;
        }
    }

    int num_sections_;
    T b0_[max_num_sections], b1_[max_num_sections], b2_[max_num_sections];
    T a0_[max_num_sections], a1_[max_num_sections];
    T x_[max_num_sections + 1][3];
};

} // namespace audible

namespace ripples {
    using SOSCoefficients = audible::SOSCoefficients;
    template <typename T, int max_num_sections>
    using SOSFilter = audible::SOSFilter<T, max_num_sections>;
}

namespace shelves {
    using SOSCoefficients = audible::SOSCoefficients;
    template <typename T, int max_num_sections>
    using SOSFilter = audible::SOSFilter<T, max_num_sections>;
}
