/*
 *      Copyright (C) 2010-2021 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include "DShowUtil.h"

#include <cassert>
#include <cstdlib>
#include <cstring>


template <class T> class FloatingAverage
{
  public:
    explicit FloatingAverage(unsigned int iNumSamples = 10) { SetNumSamples(iNumSamples); }

    ~FloatingAverage() { std::free(m_Samples); }

    void SetNumSamples(unsigned int iNumSamples)
    {
        if (iNumSamples > m_NumSamplesAlloc)
        {
            m_Samples = reinterpret_cast<T *>(std::realloc(m_Samples, iNumSamples * sizeof(T)));
            m_NumSamplesAlloc = iNumSamples;
        }
        if (iNumSamples > m_NumSamples) {
            std::memset(m_Samples + m_NumSamples, 0, sizeof(T) * (iNumSamples - m_NumSamples));
        }
        m_NumSamples = iNumSamples;
    }

    void Sample(T fSample)
    {
        m_Samples[m_CurrentSample] = fSample;
        if (++m_CurrentSample >= m_NumSamples)
        {
            m_CurrentSample = 0;
        }
    }

    T Average() const
    {
        T fAverage{ 0 };
        for (unsigned int i{ 0U }; i < m_NumSamples; ++i)
        {
            fAverage += m_Samples[i] / m_NumSamples;
        }
        return fAverage;
    }

    T Minimum() const
    {
        T min = m_Samples[0U];
        for (unsigned int i{ 1U }; i < m_NumSamples; ++i)
        {
            if (m_Samples[i] < min)
            {
                min = m_Samples[i];
            }
        }
        return min;
    }

    T AbsMinimum() const
    {
        T min = m_Samples[0U];
        for (unsigned int i{ 1U }; i < m_NumSamples; ++i)
        {
            if ((std::abs)(m_Samples[i]) < (std::abs)(min)) {
                min = m_Samples[i];
            }
        }
        return min;
    }

    T Maximum() const
    {
        T max = m_Samples[0U];
        for (unsigned int i{ 1U }; i < m_NumSamples; ++i)
        {
            if (m_Samples[i] > max)
            {
                max = m_Samples[i];
            }
        }
        return max;
    }

    T AbsMaximum() const
    {
        T max = m_Samples[0U];
        for (unsigned int i{ 1U }; i < m_NumSamples; ++i)
        {
            if ((std::abs)(m_Samples[i]) > (std::abs)(max)) {
                max = m_Samples[i];
            }
        }
        return max;
    }

    void OffsetValues(T value) const
    {
        for (unsigned int i{ 0U }; i < m_NumSamples; ++i)
        {
            m_Samples[i] += value;
        }
    }

    [[nodiscard]] constexpr unsigned int CurrentSample() const noexcept { return m_CurrentSample; }

  private:
    T *m_Samples{ nullptr };
    unsigned int m_NumSamples{ 0U };
    unsigned int m_NumSamplesAlloc{ 0U };
    unsigned int m_CurrentSample{ 0U };
};
