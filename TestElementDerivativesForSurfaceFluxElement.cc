// -*- C++ -*-
#include "Definitions.h"
#include "Quadrature.h"
#include "Utilities.h"
#include "ChemoElectroMechanicalTriangle.h"
#include "MaterialModelLithiumIronPhosphate.h"
#include "SurfaceEnergyElement.h"
#include "SurfaceFluxElement.h"
#include "Assemblers.h"
#include "MeshUtilities.h"
//#include "SolverNewtonRaphson.h"
#include "../../core/PostProcessorVtk.h"
#include <Eigen/LU>
#include <Eigen/SuperLUSupport>

const unsigned int numberOfQuadraturePointsForTriangle                  = 3;
const unsigned int numberOfQuadraturePointsForSurfaceFlux								= 5;
typedef MaterialModels::PhaseFieldBatteryModel2D																																MaterialModel;
typedef MaterialModels::PhaseFieldBatteryModel2D::MaterialParameters																						MaterialParameters;
typedef MaterialModels::EmptyInternalVariables												InternalVariables;
typedef Elements::TriangleForBatterySimulations::LinearChemoMechanical<MaterialModel,
                                                                      numberOfQuadraturePointsForTriangle>			VolumeElement;
typedef Elements::TriangleForBatterySimulations::Properties																											VolumeElementProperties;
typedef Elements::SurfaceGammaElement::LinearTwoNodeSurfaceEnergyElement																				SurfaceEnergyElement;
typedef Elements::SurfaceGammaElement::Properties																																SurfaceEnergyElementProperties;
typedef Elements::SurfaceFluxElementDoyle::LinearTwoNodeSurfaceFluxElement<numberOfQuadraturePointsForSurfaceFlux>		SurfaceFluxElement;
typedef Elements::SurfaceFluxElementDoyle::Properties																																SurfaceFluxElementProperties;
typedef SingleElementMesh<VolumeElement>                             Mesh;
typedef Assemblers::AssemblerChemoMechanicalProblem<VolumeElement,
																										SurfaceEnergyElement,
																										SurfaceFluxElement>																					Assembler;
typedef VolumeElement::Node                                    Node;
typedef VolumeElement::PotentialVector												 PotentialVector;
typedef VolumeElement::Forces																	 Forces;
typedef VolumeElement::StiffnessMatrix												 StiffnessMatrix;
typedef VolumeElement::Point                                   Point;
typedef VolumeElement::PrimitiveVariables											 PrimitiveVariablesForVolumeElement;
typedef SurfaceFluxElement::PrimitiveVariables								 PrimitiveVariablesForSurfaceElement;
typedef VolumeElement::TotalVariableVector										 TotalVariableVector;
typedef VolumeElement::NTNMatrix															 NTNMatrix;
typedef VolumeElement::Stress                                  Stress;
typedef VolumeElement::Strain                                  Strain;
typedef MaterialModel::TangentMatrix													 TangentMatrix;
typedef Eigen::SparseMatrix<double>                            SparseMatrix;
typedef Eigen::SuperLU<SparseMatrix>                           SuperLUofSparseMatrix;
//typedef Solvers::NewtonRaphsonEigen<Assembler, SurfaceFluxElement> SolverNewtonRaphson;

static const size_t TotalDegreesOfFreedom = VolumeElement::TotalDegreesOfFreedom;
//static const size_t MechanicalDegreesOfFreedom = VolumeElement::MechanicalDegreesOfFreedom;


int main(int arc, char *argv[]) {

  ignoreUnusedVariables(arc);
	
	// input all material parameters
	MaterialParameters materialParameters;
	materialParameters.R = 8.3144598; // J/(mol.K)
	materialParameters.T = 298; // K
	materialParameters.cmax = 2.29e4; // mol/m^3
	materialParameters.Omega = 11.2e3; // J/mol
	materialParameters.kappa = 0.022; // Jm^2/mol
	materialParameters.C11 = 157.4; // Pa
	materialParameters.C22 = 175.8; // Pa
	materialParameters.C33 = 154.9; // Pa
	materialParameters.C44 = 37.; // Pa
	materialParameters.C55 = 49.05; // Pa
	materialParameters.C66 = 51.6; // Pa
	materialParameters.C12 = 51.2; // Pa
	materialParameters.C23 = 53.25; //  Pa
	materialParameters.C31 = 32.7; // Pa
	materialParameters.eps0aa = 0.05;
	materialParameters.eps0bb = 0.028;
	materialParameters.eps0cc = -0.025;
	materialParameters.Da = 10; // m^2/s
	materialParameters.Db = 10; // m^2/s
	
	MaterialModel materialModel(materialParameters);
	
	double thickness = 1.; // m
	double cmax = 2.29e4; // mol/m^3
	double gamma_ac_FePO4 = 0.;
	double gamma_ac_LiFePO4 = 0.26; // J/m^2
	double gamma_bc_FePO4 = 0.;
	double gamma_bc_LiFePO4 = -0.4; // J/m^2
	double R = 8.3144598; // J/(mol.K)
	double T = 298; // K
	double F = 96485.33289; // C/mol (Faraday constant)
	double k = 1.e-2; // A/m^2
	
	VolumeElementProperties volElementProperties(thickness,cmax);
	SurfaceEnergyElementProperties surfaceEnergyElementPropertiesAC(thickness,cmax,gamma_ac_FePO4,gamma_ac_LiFePO4);
	SurfaceEnergyElementProperties surfaceEnergyElementPropertiesBC(thickness,cmax,gamma_bc_FePO4,gamma_bc_LiFePO4);
	SurfaceFluxElementProperties surfaceFluxElementProperties(thickness,R,T,F,cmax,k);
	
	const QuadratureRule<2, numberOfQuadraturePointsForTriangle> quadratureRuleForVolume =
	Quadrature::buildSimplicialQuadrature<2, numberOfQuadraturePointsForTriangle>();
	const QuadratureRule<1, numberOfQuadraturePointsForSurfaceFlux> quadratureRuleForSurface =
	Quadrature::buildGaussianQuadrature<1, numberOfQuadraturePointsForSurfaceFlux>();
	
	Node node1;
	Node node2;
	Node node3;
	
	node1._position[0] = 0.;
	node1._position[1] = 0.;
	node2._position[0] = 0.;
	node2._position[1] = 1.;
	node3._position[0] = 1.;
	node3._position[1] = 0.;
	
	node1._id = 0;
	node2._id = 1;
	node3._id = 2;
	
	array<Node, 3> elementNodes;
	elementNodes[0] = node1;
	elementNodes[1] = node2;
	elementNodes[2] = node3;
	
	array<Node, 2> elementNodesForSurface;
	elementNodesForSurface[0] = node1;
	elementNodesForSurface[1] = node2;
	
	SurfaceFluxElement element(elementNodesForSurface,surfaceFluxElementProperties,&quadratureRuleForSurface);
	
	PrimitiveVariablesForSurfaceElement primitives;
	
	for(unsigned int i = 0; i < 2; i++){
		primitives[i] = 1e-3 * TotalVariableVector::Random();
		primitives[i](2) += 0.5;
	}
	
	//cout << primitives << endl;
	
	// STIFFNESS MATRIX COMPONENTS FOR CURRENT EQUATION
	
	double timeStep = 0.01;
	double phi = 0.3;
	double perturbation = 1e-8;
	double tolerance = 1e-4;
	
	Matrix<double, 5, 1> currentStiffness = element.computeStiffnessMatrixComponentsForCurrentEquation(primitives,phi,timeStep);
	Matrix<double, 5, 1> perturbedCurrentStiffness = element.computeStiffnessMatrixComponentsForCurrentEquation(primitives,phi,timeStep);
	perturbedCurrentStiffness.fill(0.);
	double boundaryCurrent = 1.;
	
	PrimitiveVariablesForSurfaceElement perturbedPrimitives = primitives;
	perturbedPrimitives[0](3) += perturbation;
	perturbedCurrentStiffness(0) = (element.computeForceForCurrentEquation(perturbedPrimitives,phi,timeStep)-element.computeForceForCurrentEquation(primitives,phi,timeStep))/perturbation;
	
	perturbedPrimitives = primitives;
	perturbedPrimitives[1](3) += perturbation;
	perturbedCurrentStiffness(1) = (element.computeForceForCurrentEquation(perturbedPrimitives,phi,timeStep)-element.computeForceForCurrentEquation(primitives,phi,timeStep))/perturbation;
	
	double dphi = phi + perturbation;
	perturbedCurrentStiffness(2) = (element.computeForceForCurrentEquation(primitives,dphi,timeStep)-element.computeForceForCurrentEquation(primitives,phi,timeStep))/perturbation;
	
	perturbedPrimitives = primitives;
	perturbedPrimitives[0](2) += perturbation;
	perturbedCurrentStiffness(3) = (element.computeForceForCurrentEquation(perturbedPrimitives,phi,timeStep)-element.computeForceForCurrentEquation(primitives,phi,timeStep))/perturbation;
	
	perturbedPrimitives = primitives;
	perturbedPrimitives[1](2) += perturbation;
	perturbedCurrentStiffness(4) = (element.computeForceForCurrentEquation(perturbedPrimitives,phi,timeStep)-element.computeForceForCurrentEquation(primitives,phi,timeStep))/perturbation;
	
	double errorCurrentStiffness = (currentStiffness-perturbedCurrentStiffness).norm()/currentStiffness.norm();
	
	cout << "the computed stiffness matrix from perturbation is: \n" << currentStiffness << endl;
	
	cout << "the computed stiffness matrix is: \n" << perturbedCurrentStiffness << endl;
	
	
	cout << "error of method computeStiffnessMatrixComponentsForCurrentEquation is: " << errorCurrentStiffness << endl;
	
	if (errorCurrentStiffness >= tolerance || std::isfinite(errorCurrentStiffness) == false) {
		cout << "Warning: element derivatives test for current equation failed." << endl << endl;
	}
	else{
		cout << "Element test derivatives passed for current equation." << endl;
	}
	
	
	// NORMAL STIFFNESS MATRIX
	
	const SurfaceFluxElement::Forces F0 = element.computeForces(primitives,phi,timeStep);
	const SurfaceFluxElement::StiffnessMatrix K0 = element.computeStiffnessMatrix(primitives,phi,timeStep);
	SurfaceFluxElement::StiffnessMatrix stiffnessMatrix;
	
	for (unsigned int n = 0; n < 2; n++) {
		for (unsigned int dof = 0; dof < 4; dof++) {
			PrimitiveVariablesForSurfaceElement perturbedPrimitives = primitives;
			perturbedPrimitives[n](dof) += perturbation;
			const SurfaceFluxElement::Forces F1 =
			element.computeForces(perturbedPrimitives, phi, timeStep);
			for (unsigned int N = 0; N < 2; N++) {
				for (unsigned int DOF = 0; DOF < 4; DOF++) {
					stiffnessMatrix(4*N+DOF,4*n+dof) = (F1[N](DOF) - F0[N](DOF)) / perturbation;
				}
			}
		}
	}
	
	cout << "the computed stiffness matrix from perturbation is: \n" << stiffnessMatrix << endl;
	
	cout << "the computed stiffness matrix is: \n" << K0 << endl;
	
	const double errorStiffness = (stiffnessMatrix - K0).norm()/K0.norm();
	
	cout << "error of method computeStiffnessMatrix = " << errorStiffness << endl;
	
	
	if (errorStiffness >= tolerance || std::isfinite(errorStiffness) == false) {
		cout << "Warning: element derivatives test failed." << endl << endl;
	}
	else{
		cout << "Element test derivatives passed for surface flux element." << endl;
	}
	
	Matrix<double, 2, 1> phiStiffness = element.computePhiStiffness(primitives,phi,timeStep);
	const SurfaceFluxElement::Forces F0perturbed = element.computeForces(primitives,dphi,timeStep);
	
	Matrix<double, 2, 1> perturbedPhiStiffness;
	perturbedPhiStiffness.fill(0.);
	
	perturbedPhiStiffness(0) = (F0perturbed[0](3)-F0[0](3))/perturbation;
	perturbedPhiStiffness(1) = (F0perturbed[1](3)-F0[1](3))/perturbation;
	
	double errorPhiStiffness = (phiStiffness-perturbedPhiStiffness).norm()/phiStiffness.norm();
	
	cout << "error of method computePhiStiffness = " << errorPhiStiffness << endl;
	
	if (errorPhiStiffness >= tolerance || std::isfinite(errorPhiStiffness) == false) {
		cout << "Warning: element derivatives test failed." << endl << endl;
	}
	else{
		cout << "Element test derivatives passed for method computePhiStiffness." << endl;
	}
	
	return 0;
	
}