#include <math.h>
#include <vector>
#include <iostream>

#include "geometry/BlockTraverser.h"
#include "geometry/SiteTraverser.h"
#include "util/utilityFunctions.h"
#include "vis/streaklineDrawer/StreaklineDrawer.h"
#include "vis/Control.h"
#include "vis/streaklineDrawer/StreakPixel.h"
#include "vis/XYCoordinates.h"

namespace hemelb
{
  namespace vis
  {
    namespace streaklinedrawer
    {
      // TODO the streaker needs to be fixed. The lines drawn differ depending on how many proccesors
      // are used to do the visualisation. This is a bug.

      // Constructor, populating fields from lattice data objects.
      StreaklineDrawer::StreaklineDrawer(const geometry::LatticeData& iLatDat,
                                         Screen& iScreen,
                                         const Viewpoint& iViewpoint, //
                                         const VisSettings& iVisSettings) :
        latDat(iLatDat), mParticles(mNeighbouringProcessors), mScreen(iScreen),
            mVelocityField(mNeighbouringProcessors), mViewpoint(iViewpoint),
            mVisSettings(iVisSettings)
      {

        site_t n = 0;

        topology::NetworkTopology* netTop = topology::NetworkTopology::Instance();

        mVelocityField.BuildVelocityField(iLatDat, this);

        shared_vs = 0;

        for (size_t m = 0; m < mNeighbouringProcessors.size(); m++)
        {
          shared_vs += mNeighbouringProcessors[m].send_vs;
        }
        if (shared_vs > 0)
        {
          s_to_send = new site_t[3 * shared_vs];
          s_to_recv = new site_t[3 * shared_vs];

          v_to_send = new float[3 * shared_vs];
          v_to_recv = new float[3 * shared_vs];
        }
        shared_vs = 0;

        for (size_t m = 0; m < mNeighbouringProcessors.size(); m++)
        {
          mNeighbouringProcessors[m].s_to_send = &s_to_send[shared_vs * 3];
          mNeighbouringProcessors[m].s_to_recv = &s_to_recv[shared_vs * 3];

          mNeighbouringProcessors[m].v_to_send = &v_to_send[shared_vs * 3];
          mNeighbouringProcessors[m].v_to_recv = &v_to_recv[shared_vs * 3];

          shared_vs += mNeighbouringProcessors[m].send_vs;

          mNeighbouringProcessors[m].send_vs = 0;
        }

        req = new MPI_Request[2 * netTop->GetProcessorCount()];

        from_proc_id_to_neigh_proc_index = new proc_t[netTop->GetProcessorCount()];

        for (proc_t m = 0; m < netTop->GetProcessorCount(); m++)
        {
          from_proc_id_to_neigh_proc_index[m] = -1;
        }
        for (size_t m = 0; m < mNeighbouringProcessors.size(); m++)
        {
          from_proc_id_to_neigh_proc_index[mNeighbouringProcessors[m].mID] = (proc_t) m;
        }

        procs = netTop->GetProcessorCount();
      }

      // Destructor
      StreaklineDrawer::~StreaklineDrawer()
      {
        delete[] from_proc_id_to_neigh_proc_index;
        delete[] req;

        if (shared_vs > 0)
        {
          delete[] v_to_recv;
          delete[] v_to_send;

          delete[] s_to_recv;
          delete[] s_to_send;
        }

      }

      // Reset the streakline drawer.
      void StreaklineDrawer::Restart()
      {
        mParticles.DeleteAll();
      }

      // Draw streaklines
      void StreaklineDrawer::StreakLines(unsigned long time_steps,
                                         unsigned long time_steps_per_cycle)
      {
        // Set the particle creation period to be every time step, unless there are >=10000
        // timesteps per cycle
        unsigned int
            particle_creation_period =
                util::NumericalFunctions::max<unsigned int>(1,
                                                            (unsigned int) (time_steps_per_cycle
                                                                / 5000));

        int timestepsBetweenStreaklinesRounded = (int) (0.5F + (float) time_steps_per_cycle
            / mVisSettings.streaklines_per_pulsatile_period);

        if ((float) (time_steps % timestepsBetweenStreaklinesRounded)
            <= (mVisSettings.streakline_length / 100.0F) * ((float) time_steps_per_cycle
                / mVisSettings.streaklines_per_pulsatile_period) && time_steps
            % particle_creation_period == 0)
        {
          createSeedParticles();
        }

        mVelocityField.counter++;

        updateVelField(0);
        communicateSiteIds();
        communicateVelocities();
        updateVelField(1);
        mParticles.ProcessParticleMovement();
        mParticles.CommunicateParticles(latDat,
                                        req,
                                        procs,
                                        mVelocityField,
                                        from_proc_id_to_neigh_proc_index);
      }

      // Render the streaklines
      PixelSet<StreakPixel>* StreaklineDrawer::Render()
      {
        int pixels_x = mScreen.GetPixelsX();
        int pixels_y = mScreen.GetPixelsY();

        const std::vector<Particle>& particles = mParticles.GetParticles();

        PixelSet<StreakPixel>* set = GetUnusedPixelSet();
        set->Clear();

        for (unsigned int n = 0; n < particles.size(); n++)
        {
          util::Vector3D<float> p1;
          p1.x = particles[n].x - float(latDat.GetXSiteCount() >> 1);
          p1.y = particles[n].y - float(latDat.GetYSiteCount() >> 1);
          p1.z = particles[n].z - float(latDat.GetZSiteCount() >> 1);

          util::Vector3D<float> p2 = mViewpoint.Project(p1);

          XYCoordinates<int> x = mScreen.TransformScreenToPixelCoordinates<int> (XYCoordinates<
              float> (p2.x, p2.y));

          if (! (x.x < 0 || x.x >= pixels_x || x.y < 0 || x.y >= pixels_y))
          {
            StreakPixel pixel(x.x, x.y, particles[n].vel, p2.z, particles[n].inletID);
            set->AddPixel(pixel);
          }
        }

        return set;
      }

      // Create seed particles to begin the streaklines.
      void StreaklineDrawer::createSeedParticles()
      {
        for (unsigned int n = 0; n < particleSeeds.size(); n++)
        {
          mParticles.AddParticle(particleSeeds[n]);
        }
      }

      // Update the velocity field.
      void StreaklineDrawer::updateVelField(int stage_id)
      {
        std::vector<Particle>& particles = mParticles.GetParticles();

        for (size_t n = particles.size() - 1; n < particles.size(); n--)
        {
          float v[2][2][2][3];
          int is_interior;

          mVelocityField.localVelField((site_t) particles[n].x,
                                       (site_t) particles[n].y,
                                       (site_t) particles[n].z,
                                       v,
                                       &is_interior,
                                       latDat,
                                       this);

          if (stage_id == 0 && !is_interior)
          {
            continue;
          }

          float interp_v[3];

          mVelocityField.GetVelocityAtPoint(particles[n].x,
                                            particles[n].y,
                                            particles[n].z,
                                            v,
                                            interp_v);

          float vel = interp_v[0] * interp_v[0] + interp_v[1] * interp_v[1] + interp_v[2]
              * interp_v[2];

          if (vel > 1.0F)
          {
            particles[n].vel = 1.0F;
            particles[n].vx = interp_v[0] / sqrtf(vel);
            particles[n].vy = interp_v[1] / sqrtf(vel);
            particles[n].vz = interp_v[2] / sqrtf(vel);

          }
          else if (vel > 1.0e-8)
          {
            particles[n].vel = sqrtf(vel);
            particles[n].vx = interp_v[0];
            particles[n].vy = interp_v[1];
            particles[n].vz = interp_v[2];

          }
          else
          {
            mParticles.DeleteParticle(n);
          }
        }
      }

      // Communicate site ids to other processors.
      void StreaklineDrawer::communicateSiteIds()
      {
        for (size_t m = 0; m < mNeighbouringProcessors.size(); m++)
        {
          MPI_Irecv(&mNeighbouringProcessors[m].recv_vs,
                    1,
                    MpiDataType(mNeighbouringProcessors[m].recv_vs),
                    mNeighbouringProcessors[m].mID,
                    30,
                    MPI_COMM_WORLD,
                    &req[procs + mNeighbouringProcessors[m].mID]);
          MPI_Isend(&mNeighbouringProcessors[m].send_vs,
                    1,
                    MpiDataType(mNeighbouringProcessors[m].send_vs),
                    mNeighbouringProcessors[m].mID,
                    30,
                    MPI_COMM_WORLD,
                    &req[mNeighbouringProcessors[m].mID]);
          MPI_Wait(&req[mNeighbouringProcessors[m].mID], MPI_STATUS_IGNORE);
        }
        for (size_t m = 0; m < mNeighbouringProcessors.size(); m++)
        {
          MPI_Wait(&req[procs + mNeighbouringProcessors[m].mID], MPI_STATUS_IGNORE);

          if (mNeighbouringProcessors[m].recv_vs > 0)
          {
            MPI_Irecv(mNeighbouringProcessors[m].s_to_recv,
                      (int) mNeighbouringProcessors[m].recv_vs * 3,
                      MpiDataType(mNeighbouringProcessors[m].s_to_recv[0]),
                      mNeighbouringProcessors[m].mID,
                      40,
                      MPI_COMM_WORLD,
                      &req[procs + mNeighbouringProcessors[m].mID]);
          }
          if (mNeighbouringProcessors[m].send_vs > 0)
          {
            MPI_Isend(mNeighbouringProcessors[m].s_to_send,
                      (int) mNeighbouringProcessors[m].send_vs * 3,
                      MpiDataType(mNeighbouringProcessors[m].s_to_send[0]),
                      mNeighbouringProcessors[m].mID,
                      40,
                      MPI_COMM_WORLD,
                      &req[mNeighbouringProcessors[m].mID]);

            MPI_Wait(&req[mNeighbouringProcessors[m].mID], MPI_STATUS_IGNORE);
          }
        }
        for (size_t m = 0; m < mNeighbouringProcessors.size(); m++)
        {
          if (mNeighbouringProcessors[m].recv_vs > 0)
          {
            MPI_Wait(&req[procs + mNeighbouringProcessors[m].mID], MPI_STATUS_IGNORE);
          }
        }
      }

      // Communicate velocities to other processors.
      void StreaklineDrawer::communicateVelocities()
      {
        for (size_t m = 0; m < mNeighbouringProcessors.size(); m++)
        {
          if (mNeighbouringProcessors[m].send_vs > 0)
          {
            MPI_Irecv(mNeighbouringProcessors[m].v_to_recv,
                      (int) mNeighbouringProcessors[m].send_vs * 3,
                      MpiDataType(mNeighbouringProcessors[m].v_to_recv[0]),
                      mNeighbouringProcessors[m].mID,
                      30,
                      MPI_COMM_WORLD,
                      &req[procs + mNeighbouringProcessors[m].mID]);
          }
        }

        for (size_t m = 0; m < mNeighbouringProcessors.size(); m++)
        {
          for (site_t n = 0; n < mNeighbouringProcessors[m].recv_vs; n++)
          {
            site_t site_i = mNeighbouringProcessors[m].s_to_recv[3 * n + 0];
            site_t site_j = mNeighbouringProcessors[m].s_to_recv[3 * n + 1];
            site_t site_k = mNeighbouringProcessors[m].s_to_recv[3 * n + 2];

            const VelocitySiteData* vel_site_data_p = mVelocityField.velSiteDataPointer(latDat,
                                                                                        site_i,
                                                                                        site_j,
                                                                                        site_k);

            if (vel_site_data_p != NULL)
            {
              mNeighbouringProcessors[m].v_to_send[3 * n + 0] = vel_site_data_p->vx;
              mNeighbouringProcessors[m].v_to_send[3 * n + 1] = vel_site_data_p->vy;
              mNeighbouringProcessors[m].v_to_send[3 * n + 2] = vel_site_data_p->vz;
            }
            else
            {
              mNeighbouringProcessors[m].v_to_send[3 * n + 0] = 0.;
              mNeighbouringProcessors[m].v_to_send[3 * n + 1] = 0.;
              mNeighbouringProcessors[m].v_to_send[3 * n + 2] = 0.;
            }
          }
          if (mNeighbouringProcessors[m].recv_vs > 0)
          {
            MPI_Isend(mNeighbouringProcessors[m].v_to_send,
                      (int) mNeighbouringProcessors[m].recv_vs * 3,
                      MpiDataType(mNeighbouringProcessors[m].v_to_send[0]),
                      mNeighbouringProcessors[m].mID,
                      30,
                      MPI_COMM_WORLD,
                      &req[mNeighbouringProcessors[m].mID]);

            MPI_Wait(&req[mNeighbouringProcessors[m].mID], MPI_STATUS_IGNORE);
          }
        }

        for (size_t m = 0; m < mNeighbouringProcessors.size(); m++)
        {
          if (mNeighbouringProcessors[m].send_vs <= 0)
            continue;

          MPI_Wait(&req[procs + mNeighbouringProcessors[m].mID], MPI_STATUS_IGNORE);

          for (site_t n = 0; n < mNeighbouringProcessors[m].send_vs; n++)
          {
            site_t neigh_i = mNeighbouringProcessors[m].s_to_send[3 * n + 0];
            site_t neigh_j = mNeighbouringProcessors[m].s_to_send[3 * n + 1];
            site_t neigh_k = mNeighbouringProcessors[m].s_to_send[3 * n + 2];

            VelocitySiteData* vel_site_data_p = mVelocityField.velSiteDataPointer(latDat,
                                                                                  neigh_i,
                                                                                  neigh_j,
                                                                                  neigh_k);

            if (vel_site_data_p != NULL)
            {
              vel_site_data_p->vx = mNeighbouringProcessors[m].v_to_recv[3 * n + 0];
              vel_site_data_p->vy = mNeighbouringProcessors[m].v_to_recv[3 * n + 1];
              vel_site_data_p->vz = mNeighbouringProcessors[m].v_to_recv[3 * n + 2];
            }
          }
        }
        for (size_t m = 0; m < mNeighbouringProcessors.size(); m++)
        {
          mNeighbouringProcessors[m].send_vs = 0;
        }
      }

    }
  }
}