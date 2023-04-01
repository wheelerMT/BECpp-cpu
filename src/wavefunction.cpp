//
// Created by mattw on 05/01/2022.
//

#include "wavefunction.h"

void Wavefunction2D::generateInitialState(const std::string &gs_phase)
{
    if (gs_phase == "polar")
    {
        std::cout << "Generating polar initial state...\n";
        std::fill(plus.begin(), plus.end(), 0.);
        std::fill(zero.begin(), zero.end(), 1.);
        std::fill(minus.begin(), minus.end(), 0.);
    } else if (gs_phase == "FM")
    {
        std::cout << "Generating ferromagnetic initial state...\n";
        std::fill(plus.begin(), plus.end(), 1 / sqrt(2.));
        std::fill(zero.begin(), zero.end(), 0.);
        std::fill(minus.begin(), minus.end(), 1 / sqrt(2.));
    }
}

Wavefunction2D::Wavefunction2D(Grid2D &grid, const std::string &gs_phase) : grid{grid}
{
    // Size arrays
    plus.resize(grid.m_xPoints * grid.m_yPoints);
    zero.resize(grid.m_xPoints * grid.m_yPoints);
    minus.resize(grid.m_xPoints * grid.m_yPoints);
    plus_k.resize(grid.m_xPoints * grid.m_yPoints);
    zero_k.resize(grid.m_xPoints * grid.m_yPoints);
    minus_k.resize(grid.m_xPoints * grid.m_yPoints);

    // Populates wavefunction components
    generateFFTPlans();
    generateInitialState(gs_phase);

    update_component_atom_num();
}

void Wavefunction2D::add_noise(const std::string &components, double mean, double stddev)
{
    // Construct random generator
    unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator{seed1};
    std::normal_distribution<double> norm_dist{mean, stddev};

    if (components == "outer")
    {
        std::cout << "Adding noise...\n";

        // Add noise to outer components
        for (int i = 0; i < grid.m_xPoints; i++)
        {
            for (int j = 0; j < grid.m_yPoints; j++)
            {
                plus[j + i * grid.m_xPoints] +=
                        std::complex<double>{norm_dist(generator), norm_dist(generator)};
                minus[j + i * grid.m_xPoints] +=
                        std::complex<double>{norm_dist(generator), norm_dist(generator)};
            }
        }
    }

    update_component_atom_num();
}

doubleArray_t Wavefunction2D::density()
{
    doubleArray_t density{};
    density.resize(grid.m_xPoints, std::vector<double>(grid.m_yPoints));

    for (int i = 0; i < grid.m_xPoints; i++)
    {
        for (int j = 0; j < grid.m_yPoints; j++)
        {
            density[i][j] += (std::pow(std::abs(plus[j + i * grid.m_yPoints]), 2)
                              + std::pow(std::abs(zero[j + i * grid.m_yPoints]), 2)
                              + std::pow(std::abs(minus[j + i * grid.m_yPoints]), 2));
        }
    }
    return density;
}

void Wavefunction2D::generateFFTPlans()
{

    forward_plus = fftw_plan_dft_2d(grid.m_xPoints, grid.m_yPoints, reinterpret_cast<fftw_complex *>(&plus[0]),
                                    reinterpret_cast<fftw_complex *>(&plus_k[0]), FFTW_FORWARD, FFTW_MEASURE);
    forward_zero = fftw_plan_dft_2d(grid.m_xPoints, grid.m_yPoints, reinterpret_cast<fftw_complex *>(&zero[0]),
                                    reinterpret_cast<fftw_complex *>(&zero_k[0]), FFTW_FORWARD, FFTW_MEASURE);
    forward_minus = fftw_plan_dft_2d(grid.m_xPoints, grid.m_yPoints, reinterpret_cast<fftw_complex *>(&minus[0]),
                                     reinterpret_cast<fftw_complex *>(&minus_k[0]), FFTW_FORWARD, FFTW_MEASURE);

    backward_plus = fftw_plan_dft_2d(grid.m_xPoints, grid.m_yPoints, reinterpret_cast<fftw_complex *>(&plus_k[0]),
                                     reinterpret_cast<fftw_complex *>(&plus[0]), FFTW_BACKWARD, FFTW_MEASURE);
    backward_zero = fftw_plan_dft_2d(grid.m_xPoints, grid.m_yPoints, reinterpret_cast<fftw_complex *>(&zero_k[0]),
                                     reinterpret_cast<fftw_complex *>(&zero[0]), FFTW_BACKWARD, FFTW_MEASURE);
    backward_minus = fftw_plan_dft_2d(grid.m_xPoints, grid.m_yPoints, reinterpret_cast<fftw_complex *>(&minus_k[0]),
                                      reinterpret_cast<fftw_complex *>(&minus[0]), FFTW_BACKWARD, FFTW_MEASURE);
}

void Wavefunction2D::fft()
{
    fftw_execute(forward_plus);
    fftw_execute(forward_zero);
    fftw_execute(forward_minus);
}

void Wavefunction2D::ifft()
{
    fftw_execute(backward_plus);
    fftw_execute(backward_zero);
    fftw_execute(backward_minus);

    // Scale output
    double size = grid.m_xPoints * grid.m_yPoints;
    std::transform(plus.begin(), plus.end(), plus.begin(), [&size](auto &c)
    { return c / size; });
    std::transform(zero.begin(), zero.end(), zero.begin(), [&size](auto &c)
    { return c / size; });
    std::transform(minus.begin(), minus.end(), minus.begin(), [&size](auto &c)
    { return c / size; });

}

double Wavefunction2D::atom_number()
{
    double atom_num = 0;
    for (int i = 0; i < grid.m_xPoints; ++i)
    {
        for (int j = 0; j < grid.m_yPoints; ++j)
        {
            atom_num += grid.m_xGridSpacing * grid.m_yGridSpacing * (std::pow(abs(plus[j + i * grid.m_yPoints]), 2) +
                                             std::pow(abs(zero[j + i * grid.m_yPoints]), 2) +
                                             std::pow(abs(minus[j + i * grid.m_yPoints]), 2));
        }
    }
    return atom_num;
}

void Wavefunction2D::update_component_atom_num()
{
    N_plus = 0.;
    N_zero = 0.;
    N_minus = 0.;

    for (int i = 0; i < grid.m_xPoints; ++i)
    {
        for (int j = 0; j < grid.m_yPoints; ++j)
        {
            N_plus += grid.m_xGridSpacing * grid.m_yGridSpacing * std::pow(abs(plus[j + i * grid.m_yPoints]), 2);
            N_zero += grid.m_xGridSpacing * grid.m_yGridSpacing * std::pow(abs(zero[j + i * grid.m_yPoints]), 2);
            N_minus += grid.m_xGridSpacing * grid.m_yGridSpacing * std::pow(abs(minus[j + i * grid.m_yPoints]), 2);
        }
    }

    N = N_plus + N_zero + N_minus;
}

double Wavefunction2D::component_atom_number(const std::string &component)
{
    double component_atom_num = 0;

    if (component == "plus")
    {
        for (int i = 0; i < grid.m_xPoints; ++i)
        {
            for (int j = 0; j < grid.m_yPoints; ++j)
            {
                component_atom_num += grid.m_xGridSpacing * grid.m_yGridSpacing * std::pow(abs(plus[j + i * grid.m_yPoints]), 2);
            }
        }

    } else if (component == "zero")
    {
        for (int i = 0; i < grid.m_xPoints; ++i)
        {
            for (int j = 0; j < grid.m_yPoints; ++j)
            {
                component_atom_num += grid.m_xGridSpacing * grid.m_yGridSpacing * std::pow(abs(zero[j + i * grid.m_yPoints]), 2);
            }
        }

    } else if (component == "minus")
    {
        for (int i = 0; i < grid.m_xPoints; ++i)
        {
            for (int j = 0; j < grid.m_yPoints; ++j)
            {
                component_atom_num += grid.m_xGridSpacing * grid.m_yGridSpacing * std::pow(abs(minus[j + i * grid.m_yPoints]), 2);
            }
        }
    }
    return component_atom_num;
}

void Wavefunction2D::apply_phase(const doubleArray_t &phase_profile)
{
    for (int i = 0; i < grid.m_xPoints; ++i)
    {
        for (int j = 0; j < grid.m_yPoints; ++j)
        {
            plus[j + i * grid.m_yPoints] *= exp(std::complex<double>{0, 1} * phase_profile[i][j]);
            zero[j + i * grid.m_yPoints] *= exp(std::complex<double>{0, 1} * phase_profile[i][j]);
            minus[j + i * grid.m_yPoints] *= exp(std::complex<double>{0, 1} * phase_profile[i][j]);
        }
    }
}

void Wavefunction2D::destroy_fft_plans()
{
    fftw_destroy_plan(forward_plus);
    fftw_destroy_plan(forward_zero);
    fftw_destroy_plan(forward_minus);
    fftw_destroy_plan(backward_plus);
    fftw_destroy_plan(backward_zero);
    fftw_destroy_plan(backward_minus);
}

void Wavefunction1D::generateInitialState(const std::string &gs_phase)
{
    if (gs_phase == "polar")
    {
        std::cout << "Generating polar initial state...\n";
        std::fill(plus.begin(), plus.end(), 0.);
        std::fill(zero.begin(), zero.end(), 1.);
        std::fill(minus.begin(), minus.end(), 0.);
    } else if (gs_phase == "FM")
    {
        std::cout << "Generating ferromagnetic initial state...\n";
        std::fill(plus.begin(), plus.end(), 1 / sqrt(2.));
        std::fill(zero.begin(), zero.end(), 0.);
        std::fill(minus.begin(), minus.end(), 1 / sqrt(2.));
    }
}

void Wavefunction1D::generateFFTPlans()
{
    forward_plus = fftw_plan_dft_1d(grid.m_xPoints, reinterpret_cast<fftw_complex *>(&plus[0]),
                                    reinterpret_cast<fftw_complex *>(&plus_k[0]), FFTW_FORWARD, FFTW_MEASURE);
    forward_zero = fftw_plan_dft_1d(grid.m_xPoints, reinterpret_cast<fftw_complex *>(&zero[0]),
                                    reinterpret_cast<fftw_complex *>(&zero_k[0]), FFTW_FORWARD, FFTW_MEASURE);
    forward_minus = fftw_plan_dft_1d(grid.m_xPoints, reinterpret_cast<fftw_complex *>(&minus[0]),
                                     reinterpret_cast<fftw_complex *>(&minus_k[0]), FFTW_FORWARD, FFTW_MEASURE);

    backward_plus = fftw_plan_dft_1d(grid.m_xPoints, reinterpret_cast<fftw_complex *>(&plus_k[0]),
                                     reinterpret_cast<fftw_complex *>(&plus[0]), FFTW_BACKWARD, FFTW_MEASURE);
    backward_zero = fftw_plan_dft_1d(grid.m_xPoints, reinterpret_cast<fftw_complex *>(&zero_k[0]),
                                     reinterpret_cast<fftw_complex *>(&zero[0]), FFTW_BACKWARD, FFTW_MEASURE);
    backward_minus = fftw_plan_dft_1d(grid.m_xPoints, reinterpret_cast<fftw_complex *>(&minus_k[0]),
                                      reinterpret_cast<fftw_complex *>(&minus[0]), FFTW_BACKWARD, FFTW_MEASURE);
}

Wavefunction1D::Wavefunction1D(Grid1D &grid, const std::string &gs_phase) : grid{grid}
{
    // Size arrays
    plus.resize(grid.m_xPoints);
    zero.resize(grid.m_xPoints);
    minus.resize(grid.m_xPoints);
    plus_k.resize(grid.m_xPoints);
    zero_k.resize(grid.m_xPoints);
    minus_k.resize(grid.m_xPoints);

    // Populates wavefunction components
    generateFFTPlans();
    generateInitialState(gs_phase);

    update_component_atom_num();
}

void Wavefunction1D::fft()
{
    fftw_execute(forward_plus);
    fftw_execute(forward_zero);
    fftw_execute(forward_minus);
}

void Wavefunction1D::ifft()
{
    fftw_execute(backward_plus);
    fftw_execute(backward_zero);
    fftw_execute(backward_minus);
}

void Wavefunction1D::destroy_fft_plans()
{
    fftw_destroy_plan(forward_plus);
    fftw_destroy_plan(forward_zero);
    fftw_destroy_plan(forward_minus);
    fftw_destroy_plan(backward_plus);
    fftw_destroy_plan(backward_zero);
    fftw_destroy_plan(backward_minus);
}

void Wavefunction1D::add_noise(const std::string &components, double mean, double stddev)
{
    // Construct random generator
    unsigned seed1 = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator{seed1};
    std::normal_distribution<double> norm_dist{mean, stddev};

    if (components == "outer")
    {
        std::cout << "Adding noise...\n";

        // Add noise to outer components
        for (int i = 0; i < grid.m_xPoints; i++)
        {
            plus[i] += std::complex<double>{norm_dist(generator), norm_dist(generator)};
            minus[i] += std::complex<double>{norm_dist(generator), norm_dist(generator)};
        }
    }

    update_component_atom_num();
}

std::vector<double> Wavefunction1D::density()
{
    std::vector<double> density{};
    density.resize(grid.m_xPoints);

    for (int i = 0; i < grid.m_xPoints; i++)
    {
        density[i] += (std::pow(std::abs(plus[i]), 2)
                       + std::pow(std::abs(zero[i]), 2)
                       + std::pow(std::abs(minus[i]), 2));
    }
    return density;
}

double Wavefunction1D::atom_number()
{
    double atom_num = 0;
    for (int i = 0; i < grid.m_xPoints; ++i)
    {
        atom_num += grid.m_xGridSpacing * (std::pow(abs(plus[i]), 2)
                               + std::pow(abs(zero[i]), 2)
                               + std::pow(abs(minus[i]), 2));
    }
    return atom_num;
}

double Wavefunction1D::component_atom_number(const std::string &component)
{
    double component_atom_num = 0;

    if (component == "plus")
    {
        for (int i = 0; i < grid.m_xPoints; ++i)
        {
            component_atom_num += grid.m_xGridSpacing *  std::pow(abs(plus[i]), 2);
        }

    } else if (component == "zero")
    {
        for (int i = 0; i < grid.m_xPoints; ++i)
        {
            component_atom_num += grid.m_xGridSpacing * std::pow(abs(zero[i]), 2);
        }

    } else if (component == "minus")
    {
        for (int i = 0; i < grid.m_xPoints; ++i)
        {
            component_atom_num += grid.m_xGridSpacing * std::pow(abs(minus[i]), 2);
        }
    }
    return component_atom_num;
}

void Wavefunction1D::update_component_atom_num()
{
    N_plus = 0.;
    N_zero = 0.;
    N_minus = 0.;

    for (int i = 0; i < grid.m_xPoints; ++i)
    {
        N_plus += grid.m_xGridSpacing * std::pow(abs(plus[i]), 2);
        N_zero += grid.m_xGridSpacing * std::pow(abs(zero[i]), 2);
        N_minus += grid.m_xGridSpacing * std::pow(abs(minus[i]), 2);
    }

    N = N_plus + N_zero + N_minus;
}
