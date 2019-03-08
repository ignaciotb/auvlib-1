#include <bathy_maps/align_map.h>
#include <bathy_maps/mesh_map.h>

#include <igl/signed_distance.h>
#include <igl/slice_mask.h>
#include <igl/opengl/glfw/Viewer.h>
#include <igl/opengl/gl.h>

#include <thread>
#include <future>

namespace align_map {

using namespace std;

double points_to_mesh_rmse(const Eigen::MatrixXd& P, const Eigen::MatrixXd& V, const Eigen::MatrixXi& F)
{
    igl::AABB<Eigen::MatrixXd, 3> tree;
    tree.init(V, F);
    return points_to_mesh_rmse(P, V, F, tree);
}

double points_to_mesh_rmse(const Eigen::MatrixXd& P, const Eigen::MatrixXd& V, const Eigen::MatrixXi& F,
                           const igl::AABB<Eigen::MatrixXd, 3>& tree)
{

    Eigen::VectorXd sqrD;
    Eigen::VectorXi I;
    Eigen::MatrixXd Q;

    cout << "Constructing tree..." << endl;

    double assoc_threshold = 1.;

    cout << "Finding closes mesh points..." << endl;

    tree.squared_distance(V, F, P, sqrD, I, Q);
    //Eigen::ArrayXd temp = (P - Q).rowwise().squaredNorm().array();
    //cout << "Temp: " << temp.transpose() << endl;
    Eigen::Array<bool, Eigen::Dynamic, 1> near = (P - Q).rowwise().squaredNorm().array() < assoc_threshold*assoc_threshold;

    int nbr_near = near.cast<int>().sum();

    cout << "Number points near surface: " << nbr_near << endl;
    cout << "Out of: " << P.rows() << endl;
    cout << "P rows: " << P.rows() << endl;

    Eigen::MatrixXd goodP = igl::slice_mask(P, near, 1);
    Eigen::MatrixXd goodQ = igl::slice_mask(Q, near, 1);

    cout << "GoodP rows: " << goodP.rows() << ", cols: " << goodP.cols() << endl;

    double rmse = sqrt((goodP - goodQ).rowwise().squaredNorm().mean());

    return rmse;
}

Eigen::MatrixXd filter_points_mesh_offset(const Eigen::MatrixXd& P, const Eigen::MatrixXd& V, const Eigen::MatrixXi& F,
                                          double offset, const igl::AABB<Eigen::MatrixXd, 3>& tree)
{

    Eigen::VectorXd sqrD;
    Eigen::VectorXi I;
    Eigen::MatrixXd Q;

    cout << "Finding closes mesh points..." << endl;

    tree.squared_distance(V, F, P, sqrD, I, Q);
    Eigen::Array<bool, Eigen::Dynamic, 1> near = sqrD.array() < offset*offset;

    int nbr_near = near.cast<int>().sum();

    cout << "Number points near surface: " << nbr_near << " out of: " << P.rows() << endl;

    Eigen::MatrixXd goodP = igl::slice_mask(P, near, 1);

    cout << "GoodP rows: " << goodP.rows() << ", cols: " << goodP.cols() << endl;

    return goodP;
}

// the reason to use xyz data here is that it is organized
// in the same way as all of our other files, though it would
// be possible to just use one big pointcloud as xyz data instead
// maybe we should do that in the future as it is more compatible
// with libigl
vector<Eigen::Matrix4d, Eigen::aligned_allocator<Eigen::Matrix4d> >
    align_maps_icp(const vector<xyz_data::Points>& maps, const vector<int>& maps_to_align, bool align_jointly)
{
    vector<Eigen::MatrixXd> eigen_maps;
    for (const xyz_data::Points& points : maps) {
        Eigen::MatrixXd P = xyz_data::to_matrix(points);
        eigen_maps.push_back(P);
    }
        
    vector<Eigen::Matrix4d, Eigen::aligned_allocator<Eigen::Matrix4d> > Ts;
    for (int ind : maps_to_align) {
        Eigen::MatrixXd V;
        Eigen::MatrixXi F;
        Eigen::Matrix2d bounds;

        Eigen::MatrixXd P = eigen_maps[ind].rowwise() - Eigen::RowVector3d(bounds(0, 0), bounds(0, 1), 0.);
        xyz_data::Points map_points;
        for (int i = 0; i < maps.size(); ++i) {
            if (i != ind) {
                map_points.insert(map_points.end(), maps[i].begin(), maps[i].end());
            }
        }
        tie(V, F, bounds) = mesh_map::mesh_from_cloud(map_points, 0.5);

        igl::AABB<Eigen::MatrixXd, 3> tree;
        tree.init(V, F);

        bool ok;
        Eigen::Affine3d T;
        tie(T, ok) = align_points_to_mesh_icp_vis(P, V, F, tree);
        if (ok) {
            eigen_maps[ind].transpose() = T*eigen_maps[ind].leftCols<3>().transpose();
            Ts.push_back(T.matrix());
        }
        else {
            Ts.push_back(Eigen::Matrix4d::Identity());
        }
    }

    return Ts;
}

pair<Eigen::Matrix4d, bool> align_points_to_mesh_icp_vis(const Eigen::MatrixXd& P, const Eigen::MatrixXd& V,
                                                         const Eigen::MatrixXi& F,
                                                         const igl::AABB<Eigen::MatrixXd, 3>& tree)
{
    igl::opengl::glfw::Viewer viewer;
	viewer.data().set_mesh(V, F);
    // Add per-vertex colors
    //viewer.data().set_colors(C);
    Eigen::MatrixXd C_jet;
    igl::jet(V.col(2), true, C_jet);
    // Add per-vertex colors
    viewer.data().set_colors(C_jet);

    viewer.data().point_size = 10;
    viewer.data().line_width = 1;

    viewer.core.is_animating = true;
    viewer.core.animation_max_fps = 30.;
    viewer.core.background_color << 1., 1., 1., 1.; // white background

    Eigen::Affine3d T;
    viewer.callback_pre_draw = [&T, &P](igl::opengl::glfw::Viewer& viewer)
    {
        Eigen::MatrixXd TP = (T*P.leftCols<3>().transpose()).transpose();
        viewer.data().set_points(TP, Eigen::RowVector3d(1., 0., 0.));
        return false;
    };

    bool ok;
    auto handle = std::async(std::launch::async, [P, V, F, tree, &T, &ok]() {
        tie(T, ok) = align_points_to_mesh_icp(P, V, F, tree, [&T](const Eigen::Affine3d& newT) { T = newT; });
    });

	viewer.launch();
    handle.get();

    return make_pair(T.matrix(), ok);
}

pair<Eigen::Matrix4d, bool> align_points_to_mesh_icp(const Eigen::MatrixXd& P, const Eigen::MatrixXd& V, const Eigen::MatrixXi& F)
{
    igl::AABB<Eigen::MatrixXd, 3> tree;
    tree.init(V, F);
    std::function<void(const Eigen::Affine3d&)> dummy;
    return align_points_to_mesh_icp(P, V, F, tree, dummy);
}

pair<Eigen::Matrix4d, bool> align_points_to_mesh_icp(const Eigen::MatrixXd& P, const Eigen::MatrixXd& V, const Eigen::MatrixXi& F,
                                                     const igl::AABB<Eigen::MatrixXd, 3>& tree)
{
    std::function<void(const Eigen::Affine3d&)> dummy;
    return align_points_to_mesh_icp(P, V, F, tree, dummy);
}

pair<Eigen::Matrix4d, bool> align_points_to_mesh_icp(const Eigen::MatrixXd& P, const Eigen::MatrixXd& V, const Eigen::MatrixXi& F,
                                                     const igl::AABB<Eigen::MatrixXd, 3>& tree,
                                                     std::function<void(const Eigen::Affine3d&)> vis_callback)
{
    Eigen::MatrixXd TP = P;
    Eigen::Affine3d T = Eigen::Affine3d::Identity();

    double change_threshold = 0.001;
    double assoc_threshold = 3.;
    int max_iterations = 100;

    double mean_dist = 0.;
    double last_dist;
    int iteration = 0;
    do {
        cout << "Doing iteration " << iteration << ", current dist: " << mean_dist << endl;
        last_dist = mean_dist;

        Eigen::Affine3d ret;
        bool ok;
        tie(ret, mean_dist, ok) = icp_iteration(TP, V, F, tree);
        if (!ok) {
            return make_pair(ret.matrix(), ok);
        }

        T = ret*T;
        TP.transpose() = T * P.leftCols<3>().transpose();

        if (vis_callback) {
            vis_callback(T);
        }

        ++iteration;
    }
    while (fabs(mean_dist - last_dist) > change_threshold && iteration < max_iterations);

    return make_pair(T.matrix(), iteration < max_iterations);
}

tuple<Eigen::Matrix4d, double, bool> icp_iteration(const Eigen::MatrixXd& P, const Eigen::MatrixXd& V, const Eigen::MatrixXi& F,
                                                   const igl::AABB<Eigen::MatrixXd, 3>& tree)
{
    double assoc_threshold = .5;

    Eigen::VectorXd sqrD;
    Eigen::VectorXi I;
    Eigen::MatrixXd Q;

    tree.squared_distance(V, F, P, sqrD, I, Q);
    Eigen::Array<bool, Eigen::Dynamic, 1> near = (P - Q).rowwise().squaredNorm().array() < assoc_threshold*assoc_threshold;

    int nbr_near = near.cast<int>().sum();
    cout << "Number points near surface: " << nbr_near << endl;
    cout << "Out of: " << P.rows() << endl;

    if (nbr_near < 100) {
        cout << "Breaking since we do not have enough points!" << endl;
        return make_tuple(Eigen::Matrix4d::Identity(), 0., false);
    }

    Eigen::MatrixXd goodP = igl::slice_mask(P, near, 1);
    Eigen::MatrixXd goodQ = igl::slice_mask(Q, near, 1);

    Eigen::Vector3d meanP = goodP.rowwise().mean().transpose();
    Eigen::Vector3d meanQ = goodQ.rowwise().mean().transpose();

    double mean_dist = (goodP - goodQ).rowwise().squaredNorm().mean();

    Eigen::Matrix3d covariance = (goodQ.transpose().colwise() - meanQ)*(goodP.rowwise() - meanP.transpose());

    // NOTE: this code is from PCL
    Eigen::JacobiSVD<Eigen::Matrix<double, 3, 3> > svd(covariance, Eigen::ComputeFullU | Eigen::ComputeFullV);
    const Eigen::Matrix3d& u = svd.matrixU();
    const Eigen::Matrix3d& v = svd.matrixV();
    Eigen::Matrix3d s = Eigen::Matrix3d::Identity();

    if (u.determinant()*v.determinant() < 0.0f) {
        cout << "Negative determinant!" << endl;
        s(2,2) = -1.0f;
    }

    cout << "Mean offset from P to Q: " << (meanQ - meanP).transpose() << endl;
    cout << "Mean P: " << meanP.transpose() << endl;
    cout << "Mean Q: " << meanQ.transpose() << endl;

    Eigen::Matrix3d r = u * s * v.transpose();
    cout << "r value\n: " << r << endl;
    cout << "r*meanP: " << (r*meanP).transpose() << endl;
    Eigen::Vector3d t = meanQ - r*meanP;
    cout << "t value: " << t.transpose() << endl;

    Eigen::Affine3d ret;
    ret(0,0)=r(0,0); ret(0,1)=r(0,1); ret(0,2)=r(0,2); ret(0,3)=t(0);
    ret(1,0)=r(1,0); ret(1,1)=r(1,1); ret(1,2)=r(1,2); ret(1,3)=t(1);
    ret(2,0)=r(2,0); ret(2,1)=r(2,1); ret(2,2)=r(2,2); ret(2,3)=t(2);
    ret(3,0)=0.0f;   ret(3,1)=0.0f;   ret(3,2)=0.0f;   ret(3,3)=1.0f;

    cout << "Current transform:\n" << ret.matrix() << endl;

    return make_tuple(ret.matrix(), mean_dist, true);
}

} // namespace align_map
